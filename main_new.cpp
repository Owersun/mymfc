#include <linux/videodev2.h>

#include "common.h"

#include "LinuxV4l2Sink.h"
#include "main_new.h"
#include "parser.h"

struct in in;

void OpenDevices()
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
                msg("\e[1;32mMFC\e[0m Found %s %s", drivername, devname);
                struct v4l2_format fmt;
                memzero(fmt);
                fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
                if (ioctl(m_iDecoderHandle, VIDIOC_TRY_FMT, &fmt) == 0) {
                  msg("\e[1;32mMFC\e[0m Direct decoding to untiled picture is supported, no conversion needed");
                  m_iConverterHandle = -1;
                  return;
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
                msg("\e[1;31mFIMC\e[0m Found %s %s", drivername, devname);
              }
          }
          if (m_iConverterHandle < 0)
            close(fd);
        }
        if (m_iDecoderHandle >= 0 && m_iConverterHandle >= 0)
          return;
      }
    }
    closedir (dir);
  }
  return;
}

bool SetupDevices(char *header, int headerSize) {
  struct v4l2_format fmt;
  struct v4l2_crop crop;
  struct V4l2SinkBuffer iBuffer;

  memzero(fmt);
  fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
  fmt.fmt.pix_mp.plane_fmt[0].sizeimage = BUFFER_SIZE;

  iMFCOutput = new CLinuxV4l2Sink(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (!iMFCOutput->Init(&fmt, INPUT_BUFFERS))
    return false;

  memzero(iBuffer);
  if (!iMFCOutput->GetBuffer(&iBuffer))
    return false;
  iBuffer.iBytesUsed[0] = headerSize;
  memcpy(iBuffer.cPlane[0], header, headerSize);
  if (!iMFCOutput->PushBuffer(&iBuffer))
    return false;
  iMFCOutput->StreamOn(VIDIOC_STREAMON);

  memzero(fmt);
  if (m_iConverterHandle < 0)
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
  iMFCCapture = new CLinuxV4l2Sink(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  iMFCCapture->Init(&fmt, 0);

  iMFCCapture->QueueAll();

  iMFCCapture->GetCrop(&crop);

  iMFCCapture->StreamOn(VIDIOC_STREAMON);

  if (m_iConverterHandle > -1) {
    iFIMCOutput = new CLinuxV4l2Sink(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
    iFIMCOutput->Init(&fmt, iMFCCapture);
    iFIMCOutput->SetCrop(&crop);
    iFIMCOutput->GetCrop(&crop);

    memzero(fmt);
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
    fmt.fmt.pix_mp.width = crop.c.width;
    fmt.fmt.pix_mp.height = crop.c.height;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    iFIMCCapture = new CLinuxV4l2Sink(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    iFIMCCapture->Init(&fmt, OUTPUT_BUFFERS);

    iFIMCCapture->QueueAll();

    iFIMCCapture->GetCrop(&crop);

    iFIMCOutput->StreamOn(VIDIOC_STREAMON);
    iFIMCCapture->StreamOn(VIDIOC_STREAMON);
  }

  return true;
}

void Cleanup() {
  msg("Starting cleanup");

  munmap(in.p, in.size);
  close(in.fd);

  delete iMFCCapture;
  delete iMFCOutput;
  delete iFIMCCapture;
  delete iFIMCOutput;
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

  OpenDevices();
  if (m_iDecoderHandle < 0) {
    err("\e[1;32mMFC\e[0m Cannot find");
    return false;
  }

  memzero(in);
  if (argc > 1)
    in.name = (char *)argv[1];
  else
    in.name = (char *)"video";
  msg("in.name: %s", in.name);
  in.fd = open(in.name, O_RDONLY);
  if (&in.fd == NULL) {
    err("Can't open input file %s!", in.name);
    return false;
  }
  fstat(in.fd, &in_stat);
  in.size = in_stat.st_size;
  in.offs = 0;
  msg("opened %s size %d", in.name, in.size);
  in.p = (char *)mmap(0, in.size, PROT_READ, MAP_SHARED, in.fd, 0);
  if (in.p == MAP_FAILED) {
    err("Failed to map input file %s", in.name);
    return false;
  }

  parser = new ParserH264;
  parser->finished = false;

  // Prepare header frame
  memzero(parser->ctx);
  char *header;
  header = new char[BUFFER_SIZE];
  (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, header, BUFFER_SIZE, &used, &frameSize, 1);
  msg("Extracted header of size %d", frameSize);

  if (!SetupDevices(header, frameSize))
    return false;

  // Reset the stream to zero position
  memzero(parser->ctx);

  msg("===START===");

  // MAIN LOOP

  memzero(iBuffer);

  timespec tim, tim2;
  tim.tv_sec = 0;
  tim.tv_nsec = 5000000L;

  clock_gettime(CLOCK_REALTIME, &startTs);

  do {

    nanosleep(&tim , &tim2);
    dbg("nanosleep 1/200");

    if (iMFCOutput->GetBuffer(&iBuffer)) {
      msg("Got buffer %d, filling", iBuffer.iIndex);
      ret = (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, (char *)iBuffer.cPlane[0], BUFFER_SIZE, &used, &frameSize, 0);
      if (ret == 0 && in.offs == in.size) {
        msg("Parser has extracted all frames");
        parser->finished = true;
        frameSize = 0;
      } else {
        msg("Extracted frame number %d of size %d", frameNumber, frameSize);
        frameNumber++;
      }
      in.offs += used;
      iBuffer.iBytesUsed[0] = frameSize;
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

    msg("Got Buffer plane1 0x%lx, plane2 0x%lx from buffer %d", (unsigned long)iBuffer.cPlane[0], (unsigned long)iBuffer.cPlane[1], iBuffer.iIndex);

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
    msg("errno: %d", errno);

  msg("===STOP===");

  clock_gettime(CLOCK_REALTIME, &endTs);
  double seconds = (double )(endTs.tv_sec - startTs.tv_sec) + (double )(endTs.tv_nsec - startTs.tv_nsec) / 1000000000;
  double fps = (double)frameNumber / seconds;
  msg("Runtime %f sec, fps: %f", seconds, fps);

  Cleanup();
  return 0;
}
