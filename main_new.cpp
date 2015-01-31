#include "system.h"
#include "main_new.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "Main"

void Cleanup() {
  if (m_cVideoCodec)
    delete m_cVideoCodec;
  if (m_cFrameData)
    delete m_cFrameData;
  if (m_cHints)
    delete m_cHints;
  if (parser)
    delete parser;
}

void intHandler(int dummy=0) {
  Cleanup();
  exit(0);
}

int main(int argc, char** argv) {
  m_cVideoCodec = NULL;
  m_cHints = NULL;
  m_cFrameData = NULL;
  parser = NULL;

  struct inputData in;
  struct stat in_stat;
  int used;
  timespec startTs, endTs;
  int frameNumber = 0;

  signal(SIGINT, intHandler);

  m_cVideoCodec = new CDVDVideoCodecMFC();
  
  memzero(in);
  if (argc > 1)
    in.name = (char *)argv[1];
  else
    in.name = (char *)"video";
  CLog::Log(LOGDEBUG, "%s::%s - in.name: %s", CLASSNAME, __func__, in.name);
  in.fd = open(in.name, O_RDONLY);
  if (&in.fd == NULL) {
    CLog::Log(LOGERROR, "Can't open input file %s!", CLASSNAME, __func__, in.name);
    Cleanup();
    return false;
  }
  fstat(in.fd, &in_stat);
  in.size = in_stat.st_size;
  in.offs = 0;
  CLog::Log(LOGDEBUG, "%s::%s - opened %s size %d", CLASSNAME, __func__, in.name, in.size);
  in.p = (char *)mmap(0, in.size, PROT_READ, MAP_SHARED, in.fd, 0);
  if (in.p == MAP_FAILED) {
    CLog::Log(LOGERROR, "Failed to map input file %s", CLASSNAME, __func__, in.name);
    Cleanup();
    return false;
  }

  parser = new ParserH264;
  parser->finished = false;

  m_cHints = new CDVDStreamInfo();
  m_cHints->software = false;
  m_cHints->codec = AV_CODEC_ID_H264;
  m_cFrameData = new char[BUFFER_SIZE];
  m_cHints->extradata = m_cFrameData;

  // Prepare header frame
  memzero(parser->ctx);
  (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, m_cFrameData, BUFFER_SIZE, &used, &m_cHints->extrasize, 1);
  CLog::Log(LOGDEBUG, "%s::%s - Extracted header of size %d", CLASSNAME, __func__, m_cHints->extrasize);

  CDVDCodecOptions options;

  if (!m_cVideoCodec->Open(*m_cHints, options)) {
    Cleanup();
    return false;
  }

  // Reset the stream to zero position
  memzero(parser->ctx);

  CLog::Log(LOGNOTICE, "%s::%s - ===START===", CLASSNAME, __func__);

  // MAIN LOOP

  clock_gettime(CLOCK_REALTIME, &startTs);

/*
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
*/
/*
      long* pts = (long*)&buffer->timestamp;
      iBuffer.iTimeStamp.tv_sec = pts[0];
      iBuffer.iTimeStamp.tv_usec = pts[1];
*/
/*
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

    if (m_iConverterHandle) {
      if (!iFIMCOutput->PushBuffer(&iBuffer))
        break;
      if (!iFIMCCapture->GetBuffer(&iBuffer))
        if (errno == EAGAIN)
          continue;
        else
          break;
    }
*/
/*
    long pts[2] = { iBuffer.iTimeStamp.tv_sec, iBuffer.iTimeStamp.tv_usec };
    *dequeuedTimestamp = *((double*)&pts[0]);;
*/
/*
    CLog::Log(LOGDEBUG, "%s::%s - Got Buffer plane1 0x%lx, plane2 0x%lx, plane3 0x%lx from buffer %d", CLASSNAME, __func__, (unsigned long)iBuffer.cPlane[0], (unsigned long)iBuffer.cPlane[1], (unsigned long)iBuffer.cPlane[2], iBuffer.iIndex);

    if (m_iConverterHandle) {
      if (!iFIMCCapture->PushBuffer(&iBuffer))
        break;
      if (!iFIMCOutput->DequeueBuffer(&iBuffer))
        break;
    }

    if (!iMFCCapture->PushBuffer(&iBuffer))
      break;

  } while (!parser->finished);
*/

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
