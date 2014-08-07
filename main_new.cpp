#include <algorithm>    // std::swap
#include <cstdlib>

#include <linux/videodev2.h>

#include "common.h"

#include "LinuxV4l2Sink.h"
#include "main_new.h"
#include "parser.h"

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

void Cleanup() {
  msg("Starting cleanup");

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
  struct in in;
  struct stat in_stat;
  int used, frameSize;
  v4l2_format *fmt;
  fmt = new v4l2_format;
  v4l2_crop *crop;
  crop = new v4l2_crop;
  struct V4l2SinkBuffer iBuffer;

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
  header = new char[STREAM_BUFFER_SIZE];
  (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, header, STREAM_BUFFER_SIZE, &used, &frameSize, 1);
  msg("Extracted header of size %d", frameSize);

  memzero(*fmt);
  fmt->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
  fmt->fmt.pix_mp.plane_fmt[0].sizeimage = STREAM_BUFFER_SIZE;
  
  iMFCOutput = new CLinuxV4l2Sink(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (!iMFCOutput->Init(fmt, STREAM_BUFFER_CNT)) 
    return false;

  memzero(iBuffer);
  if (!iMFCOutput->GetBuffer(&iBuffer))
    return false;
  iBuffer.iBytesUsed[0] = frameSize;
  memcpy(iBuffer.cPlane[0], header, frameSize);
  if (!iMFCOutput->PushBuffer(&iBuffer))
    return false;
  iMFCOutput->StreamOn(VIDIOC_STREAMON);

  memzero(*fmt);
  if (m_iConverterHandle < 0)
    fmt->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
  iMFCCapture = new CLinuxV4l2Sink(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  iMFCCapture->Init(fmt, 0);

  iMFCCapture->QueueAll();

  iMFCCapture->GetCrop(crop);
  
  iMFCCapture->StreamOn(VIDIOC_STREAMON);

  if (m_iConverterHandle > -1) {
    iFIMCOutput = new CLinuxV4l2Sink(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
    iFIMCOutput->Init(fmt, iMFCCapture);
    iFIMCOutput->SetCrop(crop);

    fmt->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
    iFIMCCapture = new CLinuxV4l2Sink(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    iFIMCCapture->Init(fmt, 3);
    iFIMCCapture->SetCrop(crop);

    iFIMCCapture->QueueAll();
    
    iFIMCOutput->StreamOn(VIDIOC_STREAMON);
    iFIMCCapture->StreamOn(VIDIOC_STREAMON);
  }
    
  delete[] header;
  header = NULL;
  delete fmt;
  fmt = NULL;
  delete crop;
  crop = NULL;

  // Reset the stream to zero position
  memzero(parser->ctx);


  msg("===START===");

  // MAIN LOOP
  timespec startTs, endTs, tim, tim2;
  int finish = false;
  int frameNumber = 0;
  int frameProcessed = 0;
  int MFCdequeuedBufferNumber = -1;
  int FIMCdequeuedBufferNumber = -1;
  int BufferToShow = -1;
  int index, ret;
  double dequeuedTimestamp;

  tim.tv_sec = 0;
  tim.tv_nsec = 20000000L; // 1/50 sec, 50 fps gap
  clock_gettime(CLOCK_REALTIME, &startTs);

  do {
//    nanosleep(&tim , &tim2);
//    dbg("nanosleep 1/50");
    memzero(iBuffer);
    if (!iMFCOutput->GetBuffer(&iBuffer))
      break;
    msg("Got buffer %d, filling", iBuffer.iIndex);
    ret = (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, (char *)iBuffer.cPlane[0], STREAM_BUFFER_SIZE, &used, &frameSize, 0);
    if (ret == 0 && in.offs == in.size) {
      msg("Parser has extracted all frames");
      parser->finished = true;
      frameSize = 0;
    } else {
      msg("Extracted frame number %d of size %d", frameNumber, frameSize);
      frameNumber++;
    }
    iBuffer.iBytesUsed[0] = frameSize;
    if (!iMFCOutput->PushBuffer(&iBuffer))
      break;

    if (!iMFCCapture->GetBuffer(&iBuffer))
      if (errno == EAGAIN)
        continue;
      else
        break;
    
    msg ("Got buffer %d, plane 1 0x%lx, plane 2 0x%lx", iBuffer.iIndex, (unsigned long)iBuffer.cPlane[0], (unsigned long)iBuffer.cPlane[1]);

    if (!iMFCCapture->PushBuffer(&iBuffer))
      break;

  } while (!parser->finished);

  msg("errno: %d", errno);

/*
  do {
    //nanosleep(&tim , &tim2);
    //dbg("nanosleep 1/30");

    index = 0;
    if (!parser->finished) {
      while (index < m_MFCOutputBuffersCount & m_v4l2MFCOutputBuffers[index].bQueue)
        index++;

      if (index >= m_MFCOutputBuffersCount) { //all input buffers are busy, dequeue needed
        ret = CLinuxV4l2::PollOutput(m_iDecoderHandle, 1000/20); // wait a 20fps gap for buffer to deqeue. POLLIN - Poll Capture, POLLOUT - Poll Output
        if (ret == V4L2_ERROR) {
          err("\e[1;32mMFC OUTPUT\e[0m PollInput Error");
          break;
        } else if (ret == V4L2_BUSY) {
          index = -1; //MFC is so busy it is not ready to recieve one more frame
          err("\e[1;32mMFC OUTPUT\e[0m buffer is BUSY");
        } else if (ret == V4L2_READY) {
          index = CLinuxV4l2::DequeueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, &dequeuedTimestamp);
          if (index < 0) {
            err("\e[1;32mMFC OUTPUT\e[0m error dequeue output buffer, got number %d, errno %d", index, errno);
            break;
          } else {
            dbg("\e[1;32mMFC OUTPUT\e[0m -> %d", index);
            m_v4l2MFCOutputBuffers[index].bQueue = false;
          }
        } else {
          err("\e[1;32mMFC OUTPUT\e[0m What the fuck is that? %d, %d", ret, errno);
          break;
        }
      }

      if (index >= 0) { //We have a buffer to write to
        // Parse frame, copy it to buffer
        ret = (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, (char *) m_v4l2MFCOutputBuffers[index].cPlane[0], STREAM_BUFFER_SIZE, &used, &frameSize, 0);
        if (ret == 0 && in.offs == in.size) {
          msg("Parser has extracted all frames");
          parser->finished = true;
          frameSize = 0;
        } else {
          frameNumber++;
          msg("Extracted frame number %d of size %d", frameNumber, frameSize);
        }
        m_v4l2MFCOutputBuffers[index].iBytesUsed[0] = frameSize;

        // Queue buffer into MFC OUTPUT queue
        ret = CLinuxV4l2::QueueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, &m_v4l2MFCOutputBuffers[index]);
        if (ret == V4L2_ERROR) {
          err("\e[1;32mMFC OUTPUT\e[0m Failed to queue buffer with index %d, errno %d", index, errno);
          break;
        } else
          dbg("\e[1;32mMFC OUTPUT\e[0m %d <-", index);
        in.offs += used;
      }
    }

    MFCdequeuedBufferNumber = CLinuxV4l2::DequeueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, &dequeuedTimestamp);
    if (MFCdequeuedBufferNumber < 0) {
      if (errno == EAGAIN) // Dequeue buffer not ready, need more data on input. EAGAIN = 11
        continue;
      err("\e[1;32mMFC CAPTURE\e[0m error dequeue output buffer, got number %d", MFCdequeuedBufferNumber);
      break;
    } else {
      dbg("\e[1;32mMFC CAPTURE\e[0m -> %d", MFCdequeuedBufferNumber);
      m_v4l2MFCCaptureBuffers[MFCdequeuedBufferNumber].bQueue = false;
    }

#ifdef USE_FIMC
//Process frame after mfc
    ret = CLinuxV4l2::QueueBuffer(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, &m_v4l2MFCCaptureBuffers[MFCdequeuedBufferNumber]);
    if (ret == V4L2_ERROR) {
      err("\e[1;33mVIDEO\e[0m Failed to queue buffer with index %d", MFCdequeuedBufferNumber);
      break;
    }
    dbg("\e[1;31mFIMC OUTPUT\e[0m %d <-", ret);

    FIMCdequeuedBufferNumber = CLinuxV4l2::DequeueBuffer(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_USERPTR, &dequeuedTimestamp);
    if (FIMCdequeuedBufferNumber < 0) {
      if (errno == EAGAIN) // Dequeue buffer not ready, need more data on input. EAGAIN = 11
        continue;
      err("\e[1;31mFIMC CAPTURE\e[0m error dequeue output buffer, got number %d", FIMCdequeuedBufferNumber);
      break;
    }
    dbg("\e[1;31mFIMC CAPTURE\e[0m -> %d", FIMCdequeuedBufferNumber);
    m_v4l2FIMCCaptureBuffers[FIMCdequeuedBufferNumber].bQueue = false;
    frameProcessed++;

    index = CLinuxV4l2::DequeueBuffer(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, &dequeuedTimestamp);
    if (index < 0) {
      if (errno == EAGAIN) // Dequeue buffer not ready, need more data on input. EAGAIN = 11
        continue;
      err("\e[1;31mFIMC OUTPUT\e[0m error dequeue output buffer, got number %d", index);
      break;
    } else {
      dbg("\e[1;31mFIMC OUTPUT\e[0m -> %d", index);
      m_v4l2MFCCaptureBuffers[index].bQueue = false;
    }
#else
    index = MFCdequeuedBufferNumber;
#endif

    ret = CLinuxV4l2::QueueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, &m_v4l2MFCCaptureBuffers[index]);
    if (ret == V4L2_ERROR) {
      err("\e[1;32mMFC CAPTURE\e[0m Failed to queue buffer with index %d, errno = %d", index, errno);
      break;
    } else
      dbg("\e[1;32mMFC CAPTURE\e[0m %d <-", ret);

#ifdef USE_FIMC
    std::swap(BufferToShow,FIMCdequeuedBufferNumber);

    ret = drmModeSetCrtc(m_iVideoHandle, modeset_list->crtc, modeset_list->bufs[BufferToShow].fb, 0, 0, &modeset_list->conn, 1, &modeset_list->mode);
    if (ret)
      err("\e[1;30mDRM\e[0m cannot flip CRTC for connector %u (%d)", modeset_list->conn, errno);
    msg("\e[1;33mDRM\e[0m Shown buffer %d", BufferToShow);

    if (FIMCdequeuedBufferNumber >=0) {
      ret = CLinuxV4l2::QueueBuffer(m_iConverterHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_USERPTR, &m_v4l2FIMCCaptureBuffers[FIMCdequeuedBufferNumber]);
      if (ret == V4L2_ERROR) {
        err("\e[1;31mFIMC CAPTURE\e[0m Failed to queue buffer with index %d, errno = %d", FIMCdequeuedBufferNumber, errno);
        break;
      } else
        dbg("\e[1;31mFIMC CAPTURE\e[0m %d <-", ret);
    }
#endif

  } while (!parser->finished);
*/

  msg("===STOP===");

  clock_gettime(CLOCK_REALTIME, &endTs);
  double seconds = (double )(endTs.tv_sec - startTs.tv_sec) + (double )(endTs.tv_nsec - startTs.tv_nsec) / 1000000000;
  double fps = (double)frameNumber / seconds;
  msg("Runtime %f sec, fps: %f", seconds, fps);

//Cleanup
  munmap(in.p, in.size);
  close(in.fd);
  
  Cleanup();
  return 0;
}
