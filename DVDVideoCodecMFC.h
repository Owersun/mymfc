#pragma once

#ifndef THIS_IS_NOT_XBMC
  #include "DVDVideoCodec.h"
  #include "DVDStreamInfo.h"
  #include "utils/BitstreamConverter.h"
  #include "xbmc/linux/LinuxV4l2Sink.h"
#else
  #include "xbmcstubs.h"
  #include "LinuxV4l2Sink.h"
#endif

#ifndef V4L2_CAP_VIDEO_M2M_MPLANE
  #define V4L2_CAP_VIDEO_M2M_MPLANE 0x00004000
#endif

#define BUFFER_SIZE        1048576 // Compressed frame size. 1080p mpeg4 10Mb/s can be >256k in size, so this is to make sure frame fits into the buffer
                                   // For very unknown reason lesser than 1Mb buffer causes MFC to corrupt its own setup, setting inapropriate values
#define INPUT_BUFFERS      3       // 3 input buffers. 2 is enough almost for everything, but on some heavy videos 3 makes a difference
#define OUTPUT_BUFFERS     3       // Triple buffering for smooth output

#define memzero(x) memset(&(x), 0, sizeof (x))

class CDVDVideoCodecMFC : public CDVDVideoCodec
{
public:
  CDVDVideoCodecMFC();
  virtual ~CDVDVideoCodecMFC();
  virtual bool Open(CDVDStreamInfo &hints, CDVDCodecOptions &options);
  virtual void Dispose();
  virtual int Decode(BYTE* pData, int iSize, double dts, double pts);
  virtual void Reset();
  bool GetPictureCommon(DVDVideoPicture* pDvdVideoPicture);
  virtual bool GetPicture(DVDVideoPicture* pDvdVideoPicture);
  virtual bool ClearPicture(DVDVideoPicture* pDvdVideoPicture);
  virtual void SetDropState(bool bDrop);
  virtual const char* GetName() { return m_name.c_str(); }; // m_name is never changed after open

protected:
  std::string m_name;

  bool m_bCodecHealthy;

  V4l2Device *m_iDecoderHandle;
  V4l2Device *m_iConverterHandle;

  CLinuxV4l2Sink *m_MFCCapture;
  CLinuxV4l2Sink *m_MFCOutput;
  CLinuxV4l2Sink *m_FIMCCapture;
  CLinuxV4l2Sink *m_FIMCOutput;

  V4l2SinkBuffer *m_Buffer;
  V4l2SinkBuffer *m_BufferNowOnScreen;

  bool m_bVideoConvert;
  CDVDStreamInfo m_hints;

  CBitstreamConverter m_converter;
  bool m_bDropPictures;

  DVDVideoPicture   m_videoBuffer;

  bool OpenDevices();
};
