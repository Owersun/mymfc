#pragma once

#ifndef THIS_IS_NOT_XBMC
  #include "DVDVideoCodec.h"
  #include "DVDStreamInfo.h"
  #include "utils/BitstreamConverter.h"
  #include "LinuxC1Codec.h"
#else
  #include "xbmcstubs.h"
  #include "LinuxC1Codec.h"
#endif

typedef struct frame_queue {
  double dts;
  double pts;
  double sort_time;
  struct frame_queue *nextframe;
} frame_queue;

class CDVDVideoCodecC1 : public CDVDVideoCodec
{
public:
  CDVDVideoCodecC1();
  virtual ~CDVDVideoCodecC1();

  // Required overrides
  virtual bool Open(CDVDStreamInfo &hints, CDVDCodecOptions &options);
  virtual void Dispose(void);
  virtual int  Decode(uint8_t *pData, int iSize, double dts, double pts);
  virtual void Reset(void);
  virtual bool GetPicture(DVDVideoPicture *pDvdVideoPicture);
  virtual void SetSpeed(int iSpeed);
  virtual void SetDropState(bool bDrop);
  virtual const char* GetName(void) { return (const char*)m_pFormatName; }

protected:
  CLinuxC1Codec  *m_Codec;
  const char     *m_pFormatName;
  DVDVideoPicture m_videobuffer;
  CDVDStreamInfo  m_hints;

  CBitstreamConverter *m_bitstream;
  bool                 m_bVideoConvert;
};
