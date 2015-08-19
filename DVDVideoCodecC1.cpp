#include "system.h"

#ifndef THIS_IS_NOT_XBMC
  #if (defined HAVE_CONFIG_H) && (!defined WIN32)
    #include "config.h"
  #endif
  #include "DVDClock.h"
  #include "DVDStreamInfo.h"
  #include "AMLCodec.h"
  #include "utils/AMLUtils.h"
  #include "utils/BitstreamConverter.h"
  #include "utils/log.h"
#endif

#include "DVDVideoCodecC1.h"

#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <dirent.h>

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CDVDVideoCodecC1"

CDVDVideoCodecC1::CDVDVideoCodecC1() :
  m_Codec(NULL),
  m_pFormatName("c1-none"),
  m_bitstream(NULL)
{
  memzero(m_videobuffer);
}

CDVDVideoCodecC1::~CDVDVideoCodecC1()
{
  Dispose();
}

bool CDVDVideoCodecC1::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  m_hints = hints;
  if (m_hints.software)
    return false;

  if (!aml_permissions())
  {
    CLog::Log(LOGERROR, "AML: no proper permission, please contact the device vendor. Skipping codec...");
    return false;
  }

  switch(m_hints.codec)
  {
    case AV_CODEC_ID_H264:
      m_pFormatName = "c1-h264";
      if (m_hints.extradata && *(uint8_t*)m_hints.extradata == 1)
      {
        m_bitstream = new CBitstreamConverter;
        m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true);
        free(m_hints.extradata);
        m_hints.extrasize = m_bitstream->GetExtraSize();
        m_hints.extradata = malloc(m_hints.extrasize);
        memcpy(m_hints.extradata, m_bitstream->GetExtraData(), m_hints.extrasize);
      }
      break;
    case AV_CODEC_ID_HEVC:
      if ((m_hints.width > 1920) || (m_hints.height > 1088))
        // 4K HEVC is supported only on Amlogic S812 chip
        return false;
      m_pFormatName = "c1-hevc";
      m_bitstream = new CBitstreamConverter();
      m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true);
      free(m_hints.extradata);
      m_hints.extrasize = m_bitstream->GetExtraSize();
      m_hints.extradata = malloc(m_hints.extrasize);
      memcpy(m_hints.extradata, m_bitstream->GetExtraData(), m_hints.extrasize);
      break;
    default:
      CLog::Log(LOGDEBUG, "%s: Unknown hints.codec(%d", CLASSNAME, m_hints.codec);
      return false;
      break;
  }

  m_aspect_ratio = m_hints.aspect;
  m_Codec = new CLinuxC1Codec();
  if (!m_Codec)
  {
    CLog::Log(LOGERROR, "%s: Failed to create C1 Amlogic Codec", CLASSNAME);
    return false;
  }

  if (!m_Codec->OpenDecoder(m_hints))
    CLog::Log(LOGERROR, "%s: Failed to open C1 Amlogic Codec", CLASSNAME);

  memzero(m_videobuffer);

  m_videobuffer.dts             = DVD_NOPTS_VALUE;
  m_videobuffer.pts             = DVD_NOPTS_VALUE;
  m_videobuffer.format          = RENDER_FMT_BYPASS;
  m_videobuffer.color_range     = 0;
  m_videobuffer.color_matrix    = 4;
  m_videobuffer.iFlags          = DVP_FLAG_ALLOCATED;
  m_videobuffer.iWidth          = m_hints.width;
  m_videobuffer.iHeight         = m_hints.height;
  m_videobuffer.iDisplayWidth   = m_videobuffer.iWidth;
  m_videobuffer.iDisplayHeight  = m_videobuffer.iHeight;

  if (m_hints.aspect > 0.0 && !m_hints.forced_aspect)
  {
    m_videobuffer.iDisplayWidth  = ((int)lrint(m_videobuffer.iHeight * m_hints.aspect)) & -3;
    if (m_videobuffer.iDisplayWidth > m_videobuffer.iWidth)
    {
      m_videobuffer.iDisplayWidth  = m_videobuffer.iWidth;
      m_videobuffer.iDisplayHeight = ((int)lrint(m_videobuffer.iWidth / m_hints.aspect)) & -3;
    }
  }

  CLog::Log(LOGINFO, "%s: Opened C1 Amlogic Codec", CLASSNAME);
  return true;
}

void CDVDVideoCodecC1::Dispose(void)
{
  if (m_Codec)
    m_Codec->CloseDecoder(), delete m_Codec, m_Codec = NULL;
  if (m_videobuffer.iFlags)
    m_videobuffer.iFlags = 0;
  if (m_bitstream)
    delete m_bitstream, m_bitstream = NULL;
}

int CDVDVideoCodecC1::Decode(uint8_t *pData, int iSize, double dts, double pts)
{
  // Handle Input, add demuxer packet to input queue, we must accept it or
  // it will be discarded as DVDPlayerVideo has no concept of "try again".
  if (pData)
  {
    if (m_bitstream)
    {
      if (!m_bitstream->Convert(pData, iSize))
        return VC_ERROR;

      pData = m_bitstream->GetConvertBuffer();
      iSize = m_bitstream->GetConvertSize();
    }
  }

  if (m_hints.ptsinvalid)
    pts = DVD_NOPTS_VALUE;

  return m_Codec->Decode(pData, iSize, dts, pts);
}

void CDVDVideoCodecC1::Reset(void)
{
  m_Codec->Reset();
}

bool CDVDVideoCodecC1::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
  if (m_Codec)
    m_Codec->GetPicture(&m_videobuffer);
  *pDvdVideoPicture = m_videobuffer;

  pDvdVideoPicture->iDisplayWidth  = pDvdVideoPicture->iWidth;
  pDvdVideoPicture->iDisplayHeight = pDvdVideoPicture->iHeight;
  if (m_aspect_ratio > 1.0 && !m_hints.forced_aspect)
  {
    pDvdVideoPicture->iDisplayWidth  = ((int)lrint(pDvdVideoPicture->iHeight * m_aspect_ratio)) & -3;
    if (pDvdVideoPicture->iDisplayWidth > pDvdVideoPicture->iWidth)
    {
      pDvdVideoPicture->iDisplayWidth  = pDvdVideoPicture->iWidth;
      pDvdVideoPicture->iDisplayHeight = ((int)lrint(pDvdVideoPicture->iWidth / m_aspect_ratio)) & -3;
    }
  }

  return true;
}

void CDVDVideoCodecC1::SetDropState(bool bDrop)
{
}

void CDVDVideoCodecC1::SetSpeed(int iSpeed)
{
  if (m_Codec)
    m_Codec->SetSpeed(iSpeed);
}
