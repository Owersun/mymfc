#include <linux/videodev2.h>

#include "common.h"

#include "LinuxV4l2Sink.h"
#include "main_new.h"
#include "parser.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "Main"

struct in in;

bool OpenDevices()
{
  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir ("/sys/class/video4linux/")) != NULL) {
    while ((ent = readdir (dir)) != NULL) {
      if (strncmp(ent->d_name, "video", 5) == 0) {
        char *p;
        char name[64];
        char devname[64];
        char sysname[64];
        char drivername[32];
        char target[1024];
        int ret;

        snprintf(sysname, 64, "/sys/class/video4linux/%s", ent->d_name);
        snprintf(name, 64, "/sys/class/video4linux/%s/name", ent->d_name);

        FILE* fp = fopen(name, "r");
        if (fgets(drivername, 32, fp) != NULL) {
          p = strchr(drivername, '\n');
          if (p != NULL)
            *p = '\0';
        } else {
          fclose(fp);
          continue;
        }
        fclose(fp);

        ret = readlink(sysname, target, sizeof(target));
        if (ret < 0)
          continue;
        target[ret] = '\0';
        p = strrchr(target, '/');
        if (p == NULL)
          continue;

        sprintf(devname, "/dev/%s", ++p);

        if (m_iDecoderHandle < 0 && strncmp(drivername, "s5p-mfc-dec", 11) == 0) {
          struct v4l2_capability cap;
          int fd = open(devname, O_RDWR | O_NONBLOCK, 0);
          if (fd > 0) {
            memzero(cap);
            ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
            if (ret == 0)
              if (cap.capabilities & V4L2_CAP_STREAMING &&
                (cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE ||
                (cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE)))) {
                m_iDecoderHandle = fd;
                CLog::Log(LOGDEBUG, "%s::%s - MFC Found %s %s", CLASSNAME, __func__, drivername, devname);
                struct v4l2_format fmt;
                memzero(fmt);
                fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
                if (ioctl(m_iDecoderHandle, VIDIOC_TRY_FMT, &fmt) == 0) {
                  CLog::Log(LOGDEBUG, "%s::%s - MFC Direct decoding to untiled picture is supported, no conversion needed", CLASSNAME, __func__);
                  m_iConverterHandle = -1;
                  return true;
                }
              }
          }
          if (m_iDecoderHandle < 0)
            close(fd);
        }
        if (m_iConverterHandle < 0 && strstr(drivername, "fimc") != NULL && strstr(drivername, "m2m") != NULL) {
          struct v4l2_capability cap;
          int fd = open(devname, O_RDWR | O_NONBLOCK, 0);
          if (fd > 0) {
            memzero(cap);
            ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
            if (ret == 0)
              if (cap.capabilities & V4L2_CAP_STREAMING &&
                (cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE ||
                (cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE)))) {
                m_iConverterHandle = fd;
                CLog::Log(LOGDEBUG, "%s::%s - FIMC Found %s %s", CLASSNAME, __func__, drivername, devname);
              }
          }
          if (m_iConverterHandle < 0)
            close(fd);
        }
        if (m_iDecoderHandle >= 0 && m_iConverterHandle >= 0)
          return true;
      }
    }
    closedir (dir);
  }
  return false;
}

bool SetupDevices(uint pixelformat, char *header, int headerSize) {
  struct v4l2_format fmt;
  struct v4l2_crop crop;
  struct V4l2SinkBuffer iBuffer;
  int finalSink = m_iDecoderHandle;
  int finalFormat = -1;

  // Test what format we can get finally
  // If converter is present, it is our final sink
  if (m_iConverterHandle > -1)
    finalSink = m_iConverterHandle;
  // Test NV12
  memzero(fmt);
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
  if (ioctl(finalSink, VIDIOC_TRY_FMT, &fmt) == 0)
    finalFormat = V4L2_PIX_FMT_NV12M;
  memzero(fmt);
  // Test YUV420
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420M;
  if (ioctl(finalSink, VIDIOC_TRY_FMT, &fmt) == 0)
    finalFormat = V4L2_PIX_FMT_YUV420M;

  if (finalFormat < 0)
    return false;

  // Create MFC Output sink (the one where encoded frames are feed)
  memzero(fmt);
  fmt.fmt.pix_mp.pixelformat = pixelformat;
  fmt.fmt.pix_mp.plane_fmt[0].sizeimage = BUFFER_SIZE;

  iMFCOutput = new CLinuxV4l2Sink(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  // Set encoded format
  if (!iMFCOutput->SetFormat(&fmt))
    return false;
  // Init with number of input buffers preset
  if (!iMFCOutput->Init(INPUT_BUFFERS))
    return false;

  // Get buffer to fill
  memzero(iBuffer);
  if (!iMFCOutput->GetBuffer(&iBuffer))
    return false;
  // Fill it with header
  iBuffer.iBytesUsed[0] = headerSize;
  memcpy(iBuffer.cPlane[0], header, headerSize);
  // Enqueue buffer
  if (!iMFCOutput->PushBuffer(&iBuffer))
    return false;

  // Create MFC Capture sink (the one from which decoded frames are read)
  memzero(fmt);
  iMFCCapture = new CLinuxV4l2Sink(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  // If there is no converter set the format on the sink
  if (!(m_iConverterHandle > -1)) {
    fmt.fmt.pix_mp.pixelformat = finalFormat;
    if (!iMFCCapture->SetFormat(&fmt))
        return false;
  }
  // Turn on MFC Output with header in it to initialize MFC with all we setup
  iMFCOutput->StreamOn(VIDIOC_STREAMON);
  // Initialize MFC Capture
  if (!iMFCCapture->Init(0))
    return false;

  // Queue all buffers (empty) to MFC Capture
  iMFCCapture->QueueAll();

  // Get MFC capture crop settings
  if (!iMFCCapture->GetCrop(&crop))
    return false;
  // Read the format of MFC Capture
  if (!iMFCCapture->GetFormat(&fmt))
    return false;

  // Turn on MFC Capture
  iMFCCapture->StreamOn(VIDIOC_STREAMON);

  // If converter is present (we need to untile the picture from format MFC produces it)
  if (m_iConverterHandle > -1) {
    // Create FIMC Output sink
    iFIMCOutput = new CLinuxV4l2Sink(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
    // Set the FIMC Output format to the one read from MFC
    if (!iFIMCOutput->SetFormat(&fmt))
      return false;
    // Init FIMC Output and link it to buffers of MFC Capture
    if (!iFIMCOutput->Init(iMFCCapture))
      return false;
    // Set the FIMC Output crop to the one read from MFC
    if (!iFIMCOutput->SetCrop(&crop))
      return false;
    // Get FIMC Output crop settings
    if (!iFIMCOutput->GetCrop(&crop))
      return false;

    // Set the same settings as FIMC Capture produced picture size
    memzero(fmt);
    fmt.fmt.pix_mp.pixelformat = finalFormat;
    fmt.fmt.pix_mp.width = crop.c.width;
    fmt.fmt.pix_mp.height = crop.c.height;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    iFIMCCapture = new CLinuxV4l2Sink(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    if (!iFIMCCapture->SetFormat(&fmt))
      return false;
    // Init FIMC capture with number of buffers preset
    if (!iFIMCCapture->Init(OUTPUT_BUFFERS))
      return false;

    // Queue all buffers (empty) to FIMC Capture
    iFIMCCapture->QueueAll();

    // Read FIMC capture crop settings
    if (!iFIMCCapture->GetCrop(&crop))
      return false;
    // Read FIMC capture format settings
    if (!iFIMCCapture->GetFormat(&fmt))
      return false;

    // Turn on FIMC Output and Capture enabling the converter
    iFIMCOutput->StreamOn(VIDIOC_STREAMON);
    iFIMCCapture->StreamOn(VIDIOC_STREAMON);
  }

  return true;
}

void Cleanup() {
  CLog::Log(LOGDEBUG, "%s::%s - Starting cleanup", CLASSNAME, __func__);

  munmap(in.p, in.size);
  close(in.fd);

  delete iMFCCapture;
  delete iMFCOutput;
  if (m_iConverterHandle > -1) {
    delete iFIMCCapture;
    delete iFIMCOutput;
    close(m_iConverterHandle);
  }
  close(m_iDecoderHandle);
}

void intHandler(int dummy=0) {
  Cleanup();
  exit(0);
}

int main(int argc, char** argv) {
  Parser* parser = NULL;
  struct stat in_stat;
  int used, frameSize;
  struct V4l2SinkBuffer iBuffer;
  timespec startTs, endTs;
  int frameNumber = 0;
  int ret;
  double dequeuedTimestamp;

  signal(SIGINT, intHandler);

  if (!OpenDevices())
    return false;

  memzero(in);
  if (argc > 1)
    in.name = (char *)argv[1];
  else
    in.name = (char *)"video";
  CLog::Log(LOGDEBUG, "%s::%s - in.name: %s", CLASSNAME, __func__, in.name);
  in.fd = open(in.name, O_RDONLY);
  if (&in.fd == NULL) {
    CLog::Log(LOGERROR, "Can't open input file %s!", CLASSNAME, __func__, in.name);
    return false;
  }
  fstat(in.fd, &in_stat);
  in.size = in_stat.st_size;
  in.offs = 0;
  CLog::Log(LOGDEBUG, "%s::%s - opened %s size %d", CLASSNAME, __func__, in.name, in.size);
  in.p = (char *)mmap(0, in.size, PROT_READ, MAP_SHARED, in.fd, 0);
  if (in.p == MAP_FAILED) {
    CLog::Log(LOGERROR, "Failed to map input file %s", CLASSNAME, __func__, in.name);
    return false;
  }

  parser = new ParserMpeg4;
  parser->finished = false;

  // Prepare header frame
  memzero(parser->ctx);
  char *header;
  header = new char[BUFFER_SIZE];
  (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, header, BUFFER_SIZE, &used, &frameSize, 1);
  CLog::Log(LOGDEBUG, "%s::%s - Extracted header of size %d", CLASSNAME, __func__, frameSize);

  // Setup devices
  if (!SetupDevices(V4L2_PIX_FMT_MPEG4, header, frameSize))
    return false;

  // Reset the stream to zero position
  memzero(parser->ctx);

  CLog::Log(LOGNOTICE, "%s::%s - ===START===", CLASSNAME, __func__);

  // MAIN LOOP

  memzero(iBuffer);

  clock_gettime(CLOCK_REALTIME, &startTs);

  do {
    if (iMFCOutput->GetBuffer(&iBuffer)) {
      CLog::Log(LOGDEBUG, "%s::%s - Got empty buffer %d from MFC Output, filling", CLASSNAME, __func__, iBuffer.iIndex);
      ret = (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, (char *)iBuffer.cPlane[0], BUFFER_SIZE, &used, &frameSize, 0);
      if (ret == 0 && in.offs == in.size) {
        CLog::Log(LOGNOTICE, "%s::%s - Parser has extracted all frames", CLASSNAME, __func__);
        parser->finished = true;
        frameSize = 0;
      } else {
        CLog::Log(LOGDEBUG, "%s::%s - Extracted frame number %d of size %d", CLASSNAME, __func__, frameNumber, frameSize);
        frameNumber++;
      }
      in.offs += used;
      iBuffer.iBytesUsed[0] = frameSize;
/*
      long* pts = (long*)&buffer->timestamp;
      iBuffer.iTimeStamp.tv_sec = pts[0];
      iBuffer.iTimeStamp.tv_usec = pts[1];
*/
      if (!iMFCOutput->PushBuffer(&iBuffer))
        break;
    } else
      if (errno != EAGAIN)
        break;

    if (!iMFCCapture->GetBuffer(&iBuffer))
      if (errno == EAGAIN)
        continue;
      else
        break;

    if (m_iConverterHandle > -1) {
      if (!iFIMCOutput->PushBuffer(&iBuffer))
        break;
      if (!iFIMCCapture->GetBuffer(&iBuffer))
        if (errno == EAGAIN)
          continue;
        else
          break;
    }
/*
    long pts[2] = { iBuffer.iTimeStamp.tv_sec, iBuffer.iTimeStamp.tv_usec };
    *dequeuedTimestamp = *((double*)&pts[0]);;
*/
    CLog::Log(LOGDEBUG, "%s::%s - Got Buffer plane1 0x%lx, plane2 0x%lx, plane3 0x%lx from buffer %d", CLASSNAME, __func__, (unsigned long)iBuffer.cPlane[0], (unsigned long)iBuffer.cPlane[1], (unsigned long)iBuffer.cPlane[2], iBuffer.iIndex);

    if (m_iConverterHandle > -1) {
      if (!iFIMCCapture->PushBuffer(&iBuffer))
        break;
      if (!iFIMCOutput->DequeueBuffer(&iBuffer))
        break;
    }

    if (!iMFCCapture->PushBuffer(&iBuffer))
      break;

  } while (!parser->finished);

  if (!parser->finished)
    CLog::Log(LOGERROR, "%s::%s - errno: %d", CLASSNAME, __func__, errno);

  CLog::Log(LOGNOTICE, "%s::%s - ===STOP===", CLASSNAME, __func__);

  clock_gettime(CLOCK_REALTIME, &endTs);
  double seconds = (double )(endTs.tv_sec - startTs.tv_sec) + (double )(endTs.tv_nsec - startTs.tv_nsec) / 1000000000;
  double fps = (double)frameNumber / seconds;
  CLog::Log(LOGNOTICE, "%s::%s - Runtime %f sec, fps: %f", CLASSNAME, __func__, seconds, fps);

  Cleanup();
  return 0;
}
