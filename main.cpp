#include "system.h"
#include "main.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "Main"

void Cleanup() {
  if (m_cVideoCodec)
    delete m_cVideoCodec;
  if (m_cHints)
    delete m_cHints;
  if (m_pDvdVideoPicture)
    delete m_pDvdVideoPicture;

  avcodec_close(codecCtx);
	avformat_close_input(&formatCtx);
}

void intHandler(int dummy=0) {
  Cleanup();
  exit(0);
}

int main(int argc, char** argv) {
  m_cVideoCodec = NULL;
  m_cHints = NULL;
  m_pDvdVideoPicture = NULL;
  formatCtx = NULL;
  codecCtx = NULL;
  codec = NULL;
  AVPacket packet;
  int videoStream = -1;
  const char* vidPath;
  timespec startTs, endTs;

  signal(SIGINT, intHandler);

  if (argc > 1)
    vidPath = (char *)argv[1];
  else
    vidPath = (char *)"video";

  av_register_all();

  if (avformat_open_input(&formatCtx, vidPath, NULL, NULL) != 0) {
    CLog::Log(LOGERROR, "%s::%s - avformat_open_input() unable to open: %s", CLASSNAME, __func__, vidPath);
    return false;
  }
  CLog::Log(LOGDEBUG, "%s::%s - video file: %s", CLASSNAME, __func__, vidPath);

  if (avformat_find_stream_info(formatCtx, NULL) < 0) {
    CLog::Log(LOGERROR, "%s::%s - avformat_find_stream_info() failed.", CLASSNAME, __func__);
    return false;
  }

  for (unsigned int i = 0; i < formatCtx->nb_streams; ++i)
    if (formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStream = i;
      break;
    }

  if (videoStream == -1) {
    CLog::Log(LOGERROR, "%s::%s - Unable to find video stream in the file.", CLASSNAME, __func__);
    return false;
  }
  CLog::Log(LOGDEBUG, "%s::%s - Video stream in the file is stream number %d", CLASSNAME, __func__, videoStream);

  codecCtx = formatCtx->streams[videoStream]->codec;
  codec = avcodec_find_decoder(codecCtx->codec_id);
  if (codec == NULL) {
    CLog::Log(LOGERROR, "%s::%s - Unsupported codec.", CLASSNAME, __func__);
    return false;
  }
  if (avcodec_open2(codecCtx, codec, NULL) < 0) {
    CLog::Log(LOGERROR, "%s::%s - Unable to open codec.", CLASSNAME, __func__);
    return false;
  }
  CLog::Log(LOGDEBUG, "%s::%s - AVCodec: %s, id %d", CLASSNAME, __func__, codec->name, codec->id);

  m_cVideoCodec = new CDVDVideoCodecC1();

  m_cHints = new CDVDStreamInfo();
  m_cHints->software = false;
  m_cHints->extradata = codecCtx->extradata;
  m_cHints->extrasize = codecCtx->extradata_size;
  m_cHints->codec     = codecCtx->codec_id;
  m_cHints->codec_tag = codecCtx->codec_tag;
  m_cHints->width     = codecCtx->width;
  m_cHints->height    = codecCtx->height;

  CLog::Log(LOGDEBUG, "%s::%s - Header of size %d", CLASSNAME, __func__, codecCtx->extradata_size);

  CDVDCodecOptions options;

  if (!m_cVideoCodec->Open(*m_cHints, options)) {
    Cleanup();
    return false;
  }


  CLog::Log(LOGNOTICE, "%s::%s - ===START===", CLASSNAME, __func__);

  // MAIN LOOP

  int frameNumber = 0;
  int ret = 0;
  m_pDvdVideoPicture = new DVDVideoPicture();

  clock_gettime(CLOCK_REALTIME, &startTs);

  while (av_read_frame(formatCtx, &packet) >= 0) {

    if (packet.stream_index != videoStream)
      continue;

    if (ret < 0) {
      CLog::Log(LOGNOTICE, "%s::%s - Parser has extracted all frames", CLASSNAME, __func__);
      break;
    }
    frameNumber++;

    CLog::Log(LOGDEBUG, "%s::%s - Extracted frame number %d of size %d", CLASSNAME, __func__, frameNumber, packet.size);

    ret = m_cVideoCodec->Decode(packet.data, packet.size, packet.pts, packet.dts);
    if (ret & VC_PICTURE)
      m_cVideoCodec->GetPicture(m_pDvdVideoPicture);

    av_free_packet(&packet);
    usleep(1000*17);
  }

  CLog::Log(LOGNOTICE, "%s::%s - ===STOP===", CLASSNAME, __func__);

  clock_gettime(CLOCK_REALTIME, &endTs);
  double seconds = (double )(endTs.tv_sec - startTs.tv_sec) + (double )(endTs.tv_nsec - startTs.tv_nsec) / 1000000000;
  double fps = (double)frameNumber / seconds;
  CLog::Log(LOGNOTICE, "%s::%s - Runtime %f sec, fps: %f", CLASSNAME, __func__, seconds, fps);

  Cleanup();
  return 0;
}
