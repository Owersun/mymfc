#pragma once

#include <queue>
#include <string>
#include <math.h>
#include <poll.h>

#ifdef __cplusplus
extern "C" {
#endif
  #include <amcodec/codec.h>
  #include <amcodec/codec_type.h>
#ifdef __cplusplus
}
#endif
#include <adec-external-ctrl.h>

#ifdef _DEBUG
  #define debug_log(...) CLog::Log(__VA_ARGS__)
#else
  #define debug_log(...)
#endif

#ifndef THIS_IS_NOT_XBMC
  #include "DVDVideoCodec.h"
  #include "DVDClock.h"
  #include "DVDStreamInfo.h"
  #include "settings/DisplaySettings.h"
  #include "utils/BitstreamConverter.h"
  #include "utils/SysfsUtils.h"
#else
  #include "xbmcstubs.h"
#endif

#define memzero(x) memset(&(x), 0, sizeof (x))

#define PTS_FREQ        90000
#define UNIT_FREQ       96000
#define AV_SYNC_THRESH  PTS_FREQ*30

#define TRICKMODE_NONE  0x00
#define TRICKMODE_I     0x01
#define TRICKMODE_FFFB  0x02

#define EXTERNAL_PTS    (1)
#define SYNC_OUTSIDE    (2)

#define HDR_BUF_SIZE 1024

#define P_PRE                     (0x02000000)
#define PLAYER_SUCCESS            (0)
#define PLAYER_FAILED             (-(P_PRE|0x01))
#define PLAYER_NOMEM              (-(P_PRE|0x02))
#define PLAYER_EMPTY_P            (-(P_PRE|0x03))
#define PLAYER_WR_FAILED          (-(P_PRE|0x21))
#define PLAYER_WR_EMPTYP          (-(P_PRE|0x22))
#define PLAYER_WR_FINISH          (P_PRE|0x1)
#define PLAYER_PTS_ERROR          (-(P_PRE|0x31))
#define PLAYER_UNSUPPORT          (-(P_PRE|0x35))
#define PLAYER_CHECK_CODEC_ERROR  (-(P_PRE|0x39))

#define RW_WAIT_TIME    (20 * 1000) // 20ms

typedef struct hdr_buf {
    char *data;
    int size;
} hdr_buf_t;

typedef struct am_packet {
    AVPacket      avpkt;
    int64_t       avpts;
    int64_t       avdts;
    int           avduration;
    int           isvalid;
    int           newflag;
    int64_t       lastpts;
    unsigned char *data;
    unsigned char *buf;
    int           data_size;
    int           buf_size;
    hdr_buf_t     *hdr;
    codec_para_t  *codec;
} am_packet_t;

typedef enum {
    AM_STREAM_UNKNOWN = 0,
    AM_STREAM_TS,
    AM_STREAM_PS,
    AM_STREAM_ES,
    AM_STREAM_RM,
    AM_STREAM_AUDIO,
    AM_STREAM_VIDEO,
} pstream_type;

typedef struct {
  bool          noblock;
  int           video_pid;
  int           video_type;
  stream_type_t stream_type;
  unsigned int  format;
  unsigned int  width;
  unsigned int  height;
  unsigned int  rate;
  unsigned int  extra;
  unsigned int  status;
  unsigned int  ratio;
  unsigned long long ratio64;
  void *param;
} aml_generic_param;

typedef struct am_private_t
{
  am_packet_t       am_pkt;
  aml_generic_param gcodec;
  codec_para_t      vcodec;

  pstream_type      stream_type;
  int               check_first_pts;

  vformat_t         video_format;
  int               video_pid;
  unsigned int      video_codec_id;
  unsigned int      video_codec_tag;
  vdec_type_t       video_codec_type;
  unsigned int      video_width;
  unsigned int      video_height;
  unsigned int      video_ratio;
  unsigned int      video_ratio64;
  unsigned int      video_rate;
  unsigned int      video_rotation_degree;
  int               flv_flag;
  int               h263_decodable;
  int               extrasize;
  uint8_t           *extradata;

  int               dumpfile;
  bool              dumpdemux;
} am_private_t;

class CLinuxC1Codec
{
public:
  CLinuxC1Codec();
  ~CLinuxC1Codec();

  bool             OpenDecoder(CDVDStreamInfo &hints);
  void             CloseDecoder();
  int              Decode(uint8_t *pData, size_t size, double dts, double pts);
  bool             GetPicture(DVDVideoPicture *pDvdVideoPicture);
  void             Reset();
  void             SetSpeed(int speed);

private:
  volatile int     m_speed;
  CDVDStreamInfo   m_hints;
  am_private_t    *am_private;
  volatile int64_t m_1st_pts;
  volatile int64_t m_cur_pts;
  volatile int64_t m_cur_pictcnt;
  volatile int64_t m_old_pictcnt;
  int64_t          m_start_dts;
  int64_t          m_start_pts;

  void             SetViewport(int width, int height);
  double           GetPlayerPtsSeconds();
  void             ShowMainVideo(const bool show);
};
