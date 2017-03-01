#include "BitstreamConverter.h"

#define BYTE unsigned char

#define fast_memcpy memcpy

#define DVD_NOPTS_VALUE    (-1LL<<52) // should be possible to represent in both double and int64_t

#define DVP_FLAG_TOP_FIELD_FIRST    0x00000001
#define DVP_FLAG_REPEAT_TOP_FIELD   0x00000002 //Set to indicate that the top field should be repeated
#define DVP_FLAG_ALLOCATED          0x00000004 //Set to indicate that this has allocated data
#define DVP_FLAG_INTERLACED         0x00000008 //Set to indicate that this frame is interlaced

#define DVP_FLAG_NOSKIP             0x00000010 // indicate this picture should never be dropped
#define DVP_FLAG_DROPPED            0x00000020 // indicate that this picture has been dropped in decoder stage, will have no data

#define DVD_CODEC_CTRL_SKIPDEINT    0x01000000 // indicate that this picture was requested to have been dropped in deint stage
#define DVD_CODEC_CTRL_NO_POSTPROC  0x02000000 // see GetCodecStats
#define DVD_CODEC_CTRL_DRAIN        0x04000000 // see GetCodecStats

#define VC_ERROR    0x00000001  // an error occured, no other messages will be returned
#define VC_BUFFER   0x00000002  // the decoder needs more data
#define VC_PICTURE  0x00000004  // the decoder got a picture, call Decode(NULL, 0) again to parse the rest of the data
#define VC_USERDATA 0x00000008  // the decoder found some userdata,  call Decode(NULL, 0) again to parse the rest of the data
#define VC_FLUSHED  0x00000010  // the decoder lost it's state, we need to restart decoding again
#define VC_DROPPED  0x00000020  // needed to identify if a picture was dropped

class CProcessInfo {
};

class CDVDCodecOptions {
};

enum ERenderFormat {
  RENDER_FMT_NONE = 0,
  RENDER_FMT_YUV420P,
  RENDER_FMT_YUV420P10,
  RENDER_FMT_YUV420P16,
  RENDER_FMT_VDPAU,
  RENDER_FMT_VDPAU_420,
  RENDER_FMT_NV12,
  RENDER_FMT_UYVY422,
  RENDER_FMT_YUYV422,
  RENDER_FMT_DXVA,
  RENDER_FMT_VAAPI,
  RENDER_FMT_VAAPINV12,
  RENDER_FMT_OMXEGL,
  RENDER_FMT_CVBREF,
  RENDER_FMT_BYPASS,
  RENDER_FMT_EGLIMG,
  RENDER_FMT_MEDIACODEC,
  RENDER_FMT_IMXMAP,
  RENDER_FMT_MMAL,
};

struct DVDVideoPicture
{
  double pts; // timestamp in seconds, used in the CDVDPlayer class to keep track of pts
  double dts;

  union
  {
    struct {
      uint8_t* data[4];      // [4] = alpha channel, currently not used
      int iLineSize[4];   // [4] = alpha channel, currently not used
    };
  };

  unsigned int iFlags;

  double       iRepeatPicture;
  double       iDuration;
  unsigned int iFrameType         : 4; // see defines above // 1->I, 2->P, 3->B, 0->Undef
  unsigned int color_matrix       : 4;
  unsigned int color_range        : 1; // 1 indicate if we have a full range of color
  unsigned int chroma_position;
  unsigned int color_primaries;
  unsigned int color_transfer;
  unsigned int extended_format;
  char         stereo_mode[32];

  int8_t* qp_table; // Quantization parameters, primarily used by filters
  int qstride;
  int qscale_type;

  unsigned int iWidth;
  unsigned int iHeight;
  unsigned int iDisplayWidth;  // width of the picture without black bars
  unsigned int iDisplayHeight; // height of the picture without black bars

  ERenderFormat format;
};

class CDVDStreamInfo {
public:
  AVCodecID codec;
  bool software;  //force software decoding
  void*        extradata; // extra data for codec to use
  unsigned int extrasize; // size of extra data
  bool ptsinvalid;  // pts cannot be trusted (avi's).
};

class CDVDVideoCodec {
public:
  CDVDVideoCodec(CProcessInfo &processInfo) { };
  virtual bool ClearPicture(DVDVideoPicture* pDvdVideoPicture)
  {
    memset(pDvdVideoPicture, 0, sizeof(DVDVideoPicture));
    return true;
  }
};

