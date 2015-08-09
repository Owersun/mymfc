#include "system.h"
#include "main.h"

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
  if (m_pDvdVideoPicture)
    delete m_pDvdVideoPicture;
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
  m_pDvdVideoPicture = NULL;

  struct inputData in;
  struct stat in_stat;
  int used;
  timespec startTs, endTs;

  signal(SIGINT, intHandler);

  m_cVideoCodec = new CDVDVideoCodecC1();

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
  m_cFrameData = new unsigned char[BUFFER_SIZE];
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

  int frameNumber = 0;
  unsigned int frameSize = 0;
  int ret = 0;
  m_pDvdVideoPicture = new DVDVideoPicture();

  clock_gettime(CLOCK_REALTIME, &startTs);

  do {

    ret = (parser->parse_stream)(&parser->ctx, in.p + in.offs, in.size - in.offs, m_cFrameData, BUFFER_SIZE, &used, &frameSize, 0);
    if (ret == 0 && in.offs == in.size) {
      CLog::Log(LOGNOTICE, "%s::%s - Parser has extracted all frames", CLASSNAME, __func__);
      parser->finished = true;
      break;
    }

    CLog::Log(LOGDEBUG, "%s::%s - Extracted frame number %d of size %d", CLASSNAME, __func__, frameNumber, frameSize);
    frameNumber++;
    in.offs += used;

    ret = m_cVideoCodec->Decode(m_cFrameData, frameSize, DVD_NOPTS_VALUE, DVD_NOPTS_VALUE);
    if (ret | VC_PICTURE)
      m_cVideoCodec->GetPicture(m_pDvdVideoPicture);

  } while (ret | VC_BUFFER && !parser->finished);

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
