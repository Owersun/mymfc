#include "system.h"

#ifndef THIS_IS_NOT_XBMC
  #if (defined HAVE_CONFIG_H) && (!defined WIN32)
    #include "config.h"
  #endif

  #include "utils/log.h"
#endif

#include "LinuxC1Codec.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CLinuxC1Codec"

static int64_t get_pts_video()
{
  int fd = open("/sys/class/tsync/pts_video", O_RDONLY);
  if (fd >= 0)
  {
    char pts_str[16];
    int size = read(fd, pts_str, sizeof(pts_str));
    close(fd);
    if (size > 0)
    {
      unsigned long pts = strtoul(pts_str, NULL, 16);
      debug_log(LOGDEBUG, "%s::%s get_pts_video: %lu", CLASSNAME, __func__, pts);
      return pts;
    }
  }
  CLog::Log(LOGERROR, "%s::%s get_pts_video: open /tsync/event error", CLASSNAME, __func__);
  return -1;
}

static vformat_t codecid_to_vformat(enum AVCodecID id)
{
  vformat_t format;
  switch (id)
  {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
      format = VFORMAT_MPEG12;
      break;
    case AV_CODEC_ID_MPEG4:
      format = VFORMAT_MPEG4;
      break;
    case AV_CODEC_ID_H264:
      format = VFORMAT_H264;
      break;
    case AV_CODEC_ID_HEVC:
      format = VFORMAT_HEVC;
      break;
    default:
      format = VFORMAT_UNSUPPORT;
      break;
  }

  CLog::Log(LOGDEBUG, "%s::%s codecid_to_vformat, id(%d) -> vformat(%d)", CLASSNAME, __func__, (int)id, format);
  return format;
}

static vdec_type_t codec_tag_to_vdec_type(unsigned int codec_tag)
{
  vdec_type_t dec_type;
  switch (codec_tag)
  {
    case CODEC_TAG_COL1:
    case CODEC_TAG_DIV3:
    case CODEC_TAG_MP43:
      // divx3.11
      dec_type = VIDEO_DEC_FORMAT_MPEG4_3;
      break;
    case CODEC_TAG_DIV4:
    case CODEC_TAG_DIVX:
      // divx4
      dec_type = VIDEO_DEC_FORMAT_MPEG4_4;
      break;
    case CODEC_TAG_XVID:
    case CODEC_TAG_xvid:
    case CODEC_TAG_XVIX:
    case CODEC_TAG_DIV5:
    case CODEC_TAG_DX50:
    case CODEC_TAG_M4S2:
    case CODEC_TAG_FMP4:
    case CODEC_TAG_DIV6:
    case CODEC_TAG_MP4V:
    case CODEC_TAG_RMP4:
    case CODEC_TAG_MPG4:
    case CODEC_TAG_mp4v:
    case AV_CODEC_ID_MPEG4:
      dec_type = VIDEO_DEC_FORMAT_MPEG4_5;
      break;
    case CODEC_TAG_AVC1:
    case CODEC_TAG_avc1:
    case CODEC_TAG_H264:
    case CODEC_TAG_h264:
    case AV_CODEC_ID_H264:
      dec_type = VIDEO_DEC_FORMAT_H264;
      break;
    case AV_CODEC_ID_HEVC:
      dec_type = VIDEO_DEC_FORMAT_HEVC;
      break;

    default:
      dec_type = VIDEO_DEC_FORMAT_UNKNOW;
      break;
  }

  CLog::Log(LOGDEBUG, "%s::%s codec_tag_to_vdec_type, codec_tag(%d) -> vdec_type(%d)", CLASSNAME, __func__, codec_tag, dec_type);
  return dec_type;
}

void codec_init_para(aml_generic_param *p_in, codec_para_t *p_out)
{
  memzero(*p_out);

  p_out->has_video          = 1;
  p_out->noblock            = p_in->noblock;
  p_out->video_pid          = p_in->video_pid;
  p_out->video_type         = p_in->video_type;
  p_out->stream_type        = p_in->stream_type;
  p_out->am_sysinfo.format  = p_in->format;
  p_out->am_sysinfo.width   = p_in->width;
  p_out->am_sysinfo.height  = p_in->height;
  p_out->am_sysinfo.rate    = p_in->rate;
  p_out->am_sysinfo.extra   = p_in->extra;
  p_out->am_sysinfo.status  = p_in->status;
  p_out->am_sysinfo.ratio   = p_in->ratio;
  p_out->am_sysinfo.ratio64 = p_in->ratio64;
  p_out->am_sysinfo.param   = p_in->param;
}

void am_packet_release(am_packet_t *pkt)
{
  if (pkt->buf != NULL)
    free(pkt->buf), pkt->buf= NULL;
  if (pkt->hdr != NULL)
  {
    if (pkt->hdr->data != NULL)
      free(pkt->hdr->data), pkt->hdr->data = NULL;
    free(pkt->hdr), pkt->hdr = NULL;
  }

  pkt->codec = NULL;
}

int check_in_pts(am_private_t *para, am_packet_t *pkt)
{
    int last_duration = 0;
    static int last_v_duration = 0;
    int64_t pts = 0;

    last_duration = last_v_duration;

    if (para->stream_type == AM_STREAM_ES) {
        if ((int64_t)AV_NOPTS_VALUE != pkt->avpts) {
            pts = pkt->avpts;

            if (codec_checkin_pts(pkt->codec, pts) != 0) {
                CLog::Log(LOGERROR, "%s::%s ERROR check in pts error!", CLASSNAME, __func__);
                return PLAYER_PTS_ERROR;
            }

        } else if ((int64_t)AV_NOPTS_VALUE != pkt->avdts) {
            pts = pkt->avdts * last_duration;

            if (codec_checkin_pts(pkt->codec, pts) != 0) {
                CLog::Log(LOGERROR, "%s::%s ERROR check in dts error!", CLASSNAME, __func__);
                return PLAYER_PTS_ERROR;
            }

            last_v_duration = pkt->avduration ? pkt->avduration : 1;
        } else {
            if (!para->check_first_pts) {
                if (codec_checkin_pts(pkt->codec, 0) != 0) {
                    CLog::Log(LOGERROR, "%s::%s ERROR check in 0 to video pts error!", CLASSNAME, __func__);
                    return PLAYER_PTS_ERROR;
                }
            }
        }
        if (!para->check_first_pts) {
            para->check_first_pts = 1;
        }
    }
    if (pts > 0)
      pkt->lastpts = pts;

    return PLAYER_SUCCESS;
}

static int write_header(am_private_t *para, am_packet_t *pkt)
{
    int write_bytes = 0, len = 0;

    if (pkt->hdr && pkt->hdr->size > 0) {
        if ((NULL == pkt->codec) || (NULL == pkt->hdr->data)) {
            CLog::Log(LOGERROR, "%s::%s [write_header]codec null!", CLASSNAME, __func__);
            return PLAYER_EMPTY_P;
        }
        while (1) {
            write_bytes = codec_write(pkt->codec, pkt->hdr->data + len, pkt->hdr->size - len);
            if (write_bytes < 0 || write_bytes > (pkt->hdr->size - len)) {
                if (-errno != AVERROR(EAGAIN)) {
                    CLog::Log(LOGERROR, "%s::%s ERROR:write header failed!", CLASSNAME, __func__);
                    return PLAYER_WR_FAILED;
                } else {
                    continue;
                }
            } else {
                len += write_bytes;
                if (len == pkt->hdr->size) {
                    break;
                }
            }
        }
    }
    return PLAYER_SUCCESS;
}

int write_av_packet(am_private_t *para, am_packet_t *pkt)
{
  //CLog::Log(LOGDEBUG, "write_av_packet, pkt->isvalid(%d), pkt->data(%p), pkt->data_size(%d)",
  //  pkt->isvalid, pkt->data, pkt->data_size);

    int write_bytes = 0, len = 0, ret;
    unsigned char *buf;
    int size;

    // do we need to check in pts or write the header ?
    if (pkt->newflag) {
        if (pkt->isvalid) {
            ret = check_in_pts(para, pkt);
            if (ret != PLAYER_SUCCESS) {
                CLog::Log(LOGERROR, "%s::%s check in pts failed", CLASSNAME, __func__);
                return PLAYER_WR_FAILED;
            }
        }
        if (write_header(para, pkt) == PLAYER_WR_FAILED) {
            CLog::Log(LOGERROR, "%s::%s write header failed!", CLASSNAME, __func__);
            return PLAYER_WR_FAILED;
        }
        pkt->newflag = 0;
    }

    buf = pkt->data;
    size = pkt->data_size ;
    if (size == 0 && pkt->isvalid) {
        pkt->isvalid = 0;
        pkt->data_size = 0;
    }

    while (size > 0 && pkt->isvalid) {
        write_bytes = codec_write(pkt->codec, buf, size);
        if (write_bytes < 0 || write_bytes > size) {
            CLog::Log(LOGDEBUG, "%s::%s write codec data failed, write_bytes(%d), errno(%d), size(%d)", CLASSNAME, __func__, write_bytes, errno, size);
            if (-errno != AVERROR(EAGAIN)) {
                CLog::Log(LOGDEBUG, "write codec data failed!");
                return PLAYER_WR_FAILED;
            } else {
                // adjust for any data we already wrote into codec.
                // we sleep a bit then exit as we will get called again
                // with the same pkt because pkt->isvalid has not been cleared.
                pkt->data += len;
                pkt->data_size -= len;
                usleep(RW_WAIT_TIME);
                CLog::Log(LOGDEBUG, "%s::%s usleep(RW_WAIT_TIME), len(%d)", CLASSNAME, __func__, len);
                return PLAYER_SUCCESS;
            }
        } else {
            // keep track of what we write into codec from this pkt
            // in case we get hit with EAGAIN.
            len += write_bytes;
            if (len == pkt->data_size) {
                pkt->isvalid = 0;
                pkt->data_size = 0;
                break;
            } else if (len < pkt->data_size) {
                buf += write_bytes;
                size -= write_bytes;
            } else {
                // writing more that we should is a failure.
                return PLAYER_WR_FAILED;
            }
        }
    }

    return PLAYER_SUCCESS;
}

static int h264_add_header(unsigned char *buf, int size, am_packet_t *pkt)
{
    if (size > HDR_BUF_SIZE)
    {
        free(pkt->hdr->data);
        pkt->hdr->data = (char *)malloc(size);
        if (!pkt->hdr->data)
            return PLAYER_NOMEM;
    }

    memcpy(pkt->hdr->data, buf, size);
    pkt->hdr->size = size;
    return PLAYER_SUCCESS;
}

static int h264_write_header(am_private_t *para, am_packet_t *pkt)
{
    CLog::Log(LOGDEBUG, "%s::%s h264_write_header", CLASSNAME, __func__);

    int ret = h264_add_header(para->extradata, para->extrasize, pkt);
    if (ret == PLAYER_SUCCESS) {
        pkt->codec = &para->vcodec;
        pkt->newflag = 1;
        ret = write_av_packet(para, pkt);
    }
    return ret;
}

static int hevc_add_header(unsigned char *buf, int size,  am_packet_t *pkt)
{
    if (size > HDR_BUF_SIZE)
    {
        free(pkt->hdr->data);
        pkt->hdr->data = (char *)malloc(size);
        if (!pkt->hdr->data)
            return PLAYER_NOMEM;
    }

    memcpy(pkt->hdr->data, buf, size);
    pkt->hdr->size = size;
    return PLAYER_SUCCESS;
}

static int hevc_write_header(am_private_t *para, am_packet_t *pkt)
{
    int ret = -1;

    if (para->extradata) {
      ret = hevc_add_header(para->extradata, para->extrasize, pkt);
    }
    if (ret == PLAYER_SUCCESS) {
      pkt->codec = &para->vcodec;
      pkt->newflag = 1;
      ret = write_av_packet(para, pkt);
    }
    return ret;
}

static int divx3_data_prefeeding(am_packet_t *pkt, unsigned w, unsigned h)
{
    unsigned i = (w << 12) | (h & 0xfff);
    unsigned char divx311_add[10] = {
        0x00, 0x00, 0x00, 0x01,
        0x20, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    divx311_add[5] = (i >> 16) & 0xff;
    divx311_add[6] = (i >> 8) & 0xff;
    divx311_add[7] = i & 0xff;

    if (pkt->hdr->data) {
        memcpy(pkt->hdr->data, divx311_add, sizeof(divx311_add));
        pkt->hdr->size = sizeof(divx311_add);
    } else {
        CLog::Log(LOGERROR, "%s::%s [divx3_data_prefeeding]No enough memory!", CLASSNAME, __func__);
        return PLAYER_FAILED;
    }
    return PLAYER_SUCCESS;
}

static int divx3_write_header(am_private_t *para, am_packet_t *pkt)
{
    CLog::Log(LOGDEBUG, "%s::%s divx3_write_header", CLASSNAME, __func__);

    divx3_data_prefeeding(pkt, para->video_width, para->video_height);

    pkt->codec = &para->vcodec;
    pkt->newflag = 1;
    write_av_packet(para, pkt);
    return PLAYER_SUCCESS;
}

static int m4s2_dx50_mp4v_add_header(unsigned char *buf, int size,  am_packet_t *pkt)
{
    if (size > pkt->hdr->size) {
        free(pkt->hdr->data), pkt->hdr->data = NULL;
        pkt->hdr->size = 0;

        pkt->hdr->data = (char*)malloc(size);
        if (!pkt->hdr->data) {
            CLog::Log(LOGERROR, "%s::%s [m4s2_dx50_add_header] NOMEM!", CLASSNAME, __func__);
            return PLAYER_FAILED;
        }
    }

    pkt->hdr->size = size;
    memcpy(pkt->hdr->data, buf, size);

    return PLAYER_SUCCESS;
}

static int m4s2_dx50_mp4v_write_header(am_private_t *para, am_packet_t *pkt)
{
    CLog::Log(LOGDEBUG, "%s::%s m4s2_dx50_mp4v_write_header", CLASSNAME, __func__);

    int ret = m4s2_dx50_mp4v_add_header(para->extradata, para->extrasize, pkt);
    if (ret == PLAYER_SUCCESS) {
        pkt->codec = &para->vcodec;
        pkt->newflag = 1;
        ret = write_av_packet(para, pkt);
    }
    return ret;
}

static int mpeg_add_header(am_private_t *para, am_packet_t *pkt)
{
    CLog::Log(LOGDEBUG, "%s::%s mpeg_add_header", CLASSNAME, __func__);

#define STUFF_BYTES_LENGTH     (256)

    int size;
    unsigned char packet_wrapper[] = {
        0x00, 0x00, 0x01, 0xe0,
        0x00, 0x00,                                /* pes packet length */
        0x81, 0xc0, 0x0d,
        0x20, 0x00, 0x00, 0x00, 0x00, /* PTS */
        0x1f, 0xff, 0xff, 0xff, 0xff, /* DTS */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    size = para->extrasize + sizeof(packet_wrapper);
    packet_wrapper[4] = size >> 8 ;
    packet_wrapper[5] = size & 0xff ;
    memcpy(pkt->hdr->data, packet_wrapper, sizeof(packet_wrapper));
    size = sizeof(packet_wrapper);
    //CLog::Log(LOGDEBUG, "[mpeg_add_header:%d]wrapper size=%d\n",__LINE__,size);
    memcpy(pkt->hdr->data + size, para->extradata, para->extrasize);
    size += para->extrasize;
    //CLog::Log(LOGDEBUG, "[mpeg_add_header:%d]wrapper+seq size=%d\n",__LINE__,size);
    memset(pkt->hdr->data + size, 0xff, STUFF_BYTES_LENGTH);
    size += STUFF_BYTES_LENGTH;
    pkt->hdr->size = size;
    //CLog::Log(LOGDEBUG, "[mpeg_add_header:%d]hdr_size=%d\n",__LINE__,size);

    pkt->codec = &para->vcodec;
    pkt->newflag = 1;
    return write_av_packet(para, pkt);

}

int pre_header_feeding(am_private_t *para, am_packet_t *pkt)
{
    int ret;

    if (pkt->hdr == NULL) {
        pkt->hdr = (hdr_buf_t*)malloc(sizeof(hdr_buf_t));
        pkt->hdr->data = (char*)malloc(HDR_BUF_SIZE);
        if (!pkt->hdr->data) {
            CLog::Log(LOGERROR, "%s::%s [pre_header_feeding] NOMEM!", CLASSNAME, __func__);
            return PLAYER_NOMEM;
        }
    }

    if (para->stream_type == AM_STREAM_ES) {
        if (VFORMAT_H264 == para->video_format || VFORMAT_H264_4K2K == para->video_format) {
            ret = h264_write_header(para, pkt);
            if (ret != PLAYER_SUCCESS) {
                return ret;
            }
        } else if (VFORMAT_HEVC == para->video_format) {
            ret = hevc_write_header(para, pkt);
            if (ret != PLAYER_SUCCESS) {
                return ret;
            }
        } else if ((VFORMAT_MPEG4 == para->video_format) && (VIDEO_DEC_FORMAT_MPEG4_3 == para->video_codec_type)) {
            ret = divx3_write_header(para, pkt);
            if (ret != PLAYER_SUCCESS) {
                return ret;
            }
        } else if ((CODEC_TAG_M4S2 == para->video_codec_tag)
                || (CODEC_TAG_DX50 == para->video_codec_tag)
                || (CODEC_TAG_mp4v == para->video_codec_tag)) {
            ret = m4s2_dx50_mp4v_write_header(para, pkt);
            if (ret != PLAYER_SUCCESS) {
                return ret;
            }
        } else if (( AV_CODEC_ID_MPEG1VIDEO == para->video_codec_id)
          || (AV_CODEC_ID_MPEG2VIDEO == para->video_codec_id)) {
            ret = mpeg_add_header(para, pkt);
            if (ret != PLAYER_SUCCESS) {
                return ret;
            }
        }
    }

    if (pkt->hdr) {
        if (pkt->hdr->data) {
            free(pkt->hdr->data);
            pkt->hdr->data = NULL;
        }
        free(pkt->hdr);
        pkt->hdr = NULL;
    }

    return PLAYER_SUCCESS;
}

CLinuxC1Codec::CLinuxC1Codec() {
  am_private = new am_private_t;
  memzero(*am_private);
}

CLinuxC1Codec::~CLinuxC1Codec() {
  delete am_private;
  am_private = NULL;
}

bool CLinuxC1Codec::OpenDecoder(CDVDStreamInfo &hints) {
  CLog::Log(LOGDEBUG, "%s::%s", CLASSNAME, __func__);

  m_speed = DVD_PLAYSPEED_NORMAL;
  m_1st_pts = 0;
  m_cur_pts = 0;
  m_cur_pictcnt = 0;
  m_old_pictcnt = 0;
  m_start_dts = 0;
  m_start_pts = 0;
  m_hints = hints;

  memzero(am_private->am_pkt);
  am_private->stream_type      = AM_STREAM_ES;
  if (hints.width == 0 || hints.height == 0)
    return false;
  am_private->video_width      = hints.width;
  am_private->video_height     = hints.height;
  am_private->video_codec_id   = hints.codec;
  am_private->video_codec_tag  = hints.codec_tag;
  am_private->video_pid        = hints.pid;

  // handle video ratio
  AVRational video_ratio       = av_d2q(1, SHRT_MAX);
  am_private->video_ratio      = ((int32_t)video_ratio.num << 16) | video_ratio.den;
  am_private->video_ratio64    = ((int64_t)video_ratio.num << 32) | video_ratio.den;

  // handle video rate
  if (hints.rfpsrate > 0 && hints.rfpsscale != 0)
  {
    // check ffmpeg r_frame_rate 1st
    am_private->video_rate = 0.5 + (float)UNIT_FREQ * hints.rfpsscale / hints.rfpsrate;
  }
  else if (hints.fpsrate > 0 && hints.fpsscale != 0)
  {
    // then ffmpeg avg_frame_rate next
    am_private->video_rate = 0.5 + (float)UNIT_FREQ * hints.fpsscale / hints.fpsrate;
  }

  // check for 1920x1080, interlaced, 25 fps
  // incorrectly reported as 50 fps (yes, video_rate == 1920)
  if (hints.width == 1920 && am_private->video_rate == 1920)
  {
    CLog::Log(LOGDEBUG, "%s::%s video_rate exception", CLASSNAME, __func__);
    am_private->video_rate = 0.5 + (float)UNIT_FREQ * 1001 / 25000;
  }

  // check for SD h264 content incorrectly reported as 60 fsp
  // mp4/avi containers :(
  if (hints.codec == AV_CODEC_ID_H264 && hints.width <= 720 && am_private->video_rate == 1602)
  {
    CLog::Log(LOGDEBUG, "%s::%s video_rate exception", CLASSNAME, __func__);
    am_private->video_rate = 0.5 + (float)UNIT_FREQ * 1001 / 24000;
  }

  // check for SD h264 content incorrectly reported as some form of 30 fsp
  // mp4/avi containers :(
  if (hints.codec == AV_CODEC_ID_H264 && hints.width <= 720)
  {
    if (am_private->video_rate >= 3200 && am_private->video_rate <= 3210)
    {
      CLog::Log(LOGDEBUG, "%s::%s video_rate exception", CLASSNAME, __func__);
      am_private->video_rate = 0.5 + (float)UNIT_FREQ * 1001 / 24000;
    }
  }

  // handle orientation
  am_private->video_rotation_degree = 0;
  if (hints.orientation == 90)
    am_private->video_rotation_degree = 1;
  else if (hints.orientation == 180)
    am_private->video_rotation_degree = 2;
  else if (hints.orientation == 270)
    am_private->video_rotation_degree = 3;

  // handle extradata
  am_private->video_format      = codecid_to_vformat(hints.codec);
  if (am_private->video_format == VFORMAT_H264) {
    if (hints.width > 1920 || hints.height > 1088) {
      am_private->video_format = VFORMAT_H264_4K2K;
    }
  }

  am_private->extrasize       = hints.extrasize;
  am_private->extradata       = (uint8_t*)malloc(hints.extrasize);
  memcpy(am_private->extradata, hints.extradata, hints.extrasize);

  if (am_private->stream_type == AM_STREAM_ES && am_private->video_codec_tag != 0)
    am_private->video_codec_type = codec_tag_to_vdec_type(am_private->video_codec_tag);
  if (am_private->video_codec_type == VIDEO_DEC_FORMAT_UNKNOW)
    am_private->video_codec_type = codec_tag_to_vdec_type(am_private->video_codec_id);

  am_private->flv_flag = 0;

  CLog::Log(LOGDEBUG, "%s::%s hints.width(%d), hints.height(%d), hints.codec(%d), hints.codec_tag(%d), hints.pid(%d)",
    CLASSNAME, __func__, hints.width, hints.height, hints.codec, hints.codec_tag, hints.pid);
  CLog::Log(LOGDEBUG, "%s::%s hints.fpsrate(%d), hints.fpsscale(%d), hints.rfpsrate(%d), hints.rfpsscale(%d), video_rate(%d)",
    CLASSNAME, __func__, hints.fpsrate, hints.fpsscale, hints.rfpsrate, hints.rfpsscale, am_private->video_rate);
  CLog::Log(LOGDEBUG, "%s::%s hints.orientation(%d), hints.forced_aspect(%d), hints.extrasize(%d)",
    CLASSNAME, __func__, hints.orientation, hints.forced_aspect, hints.extrasize);

  // default video codec params
  am_private->gcodec.noblock     = 0;
  am_private->gcodec.video_pid   = am_private->video_pid;
  am_private->gcodec.video_type  = am_private->video_format;
  am_private->gcodec.stream_type = STREAM_TYPE_ES_VIDEO;
  am_private->gcodec.format      = am_private->video_codec_type;
  am_private->gcodec.width       = am_private->video_width;
  am_private->gcodec.height      = am_private->video_height;
  am_private->gcodec.rate        = am_private->video_rate;
  am_private->gcodec.ratio       = am_private->video_ratio;
  am_private->gcodec.ratio64     = am_private->video_ratio64;
  am_private->gcodec.param       = NULL;

  switch(am_private->video_format)
  {
    case VFORMAT_MPEG4:
      am_private->gcodec.param = (void*)EXTERNAL_PTS;
      break;
    case VFORMAT_H264:
    case VFORMAT_H264MVC:
      am_private->gcodec.format = VIDEO_DEC_FORMAT_H264;
      am_private->gcodec.param  = (void*)EXTERNAL_PTS;
      // h264 in an avi file
      if (m_hints.ptsinvalid)
        am_private->gcodec.param = (void*)(EXTERNAL_PTS | SYNC_OUTSIDE);
      break;
    case VFORMAT_H264_4K2K:
      am_private->gcodec.format = VIDEO_DEC_FORMAT_H264_4K2K;
      am_private->gcodec.param  = (void*)EXTERNAL_PTS;
      // h264 in an avi file
      if (m_hints.ptsinvalid)
        am_private->gcodec.param = (void*)(EXTERNAL_PTS | SYNC_OUTSIDE);
      break;
    case VFORMAT_HEVC:
      am_private->gcodec.format = VIDEO_DEC_FORMAT_HEVC;
      am_private->gcodec.param  = (void*)EXTERNAL_PTS;
      if (m_hints.ptsinvalid)
        am_private->gcodec.param = (void*)(EXTERNAL_PTS | SYNC_OUTSIDE);
      break;
    default:
      break;
  }
  am_private->gcodec.param = (void *)((unsigned int)am_private->gcodec.param | (am_private->video_rotation_degree << 16));

  // translate from generic to firmware version dependent
  codec_init_para(&am_private->gcodec, &am_private->vcodec);

  int ret = codec_init(&am_private->vcodec);
  if (ret != CODEC_ERROR_NONE)
  {
    CLog::Log(LOGERROR, "%s::%s codec init failed, ret=0x%x", CLASSNAME, __func__, -ret);
    return false;
  }

  // make sure we are not stuck in pause (amcodec bug)
  codec_resume(&am_private->vcodec);
  codec_set_cntl_mode(&am_private->vcodec, TRICKMODE_NONE);

  codec_set_cntl_avthresh(&am_private->vcodec, AV_SYNC_THRESH);
  codec_set_cntl_syncthresh(&am_private->vcodec, 0);
  // disable tsync, we are playing video disconnected from audio.
  SysfsUtils::SetInt("/sys/class/tsync/enable", 0);

  am_private->am_pkt.codec = &am_private->vcodec;
  pre_header_feeding(am_private, &am_private->am_pkt);

  SetSpeed(m_speed);

  SetViewport(hints.width, hints.height);
  ShowMainVideo(true);

  return true;
}

void CLinuxC1Codec::ShowMainVideo(const bool show)
{
  SysfsUtils::SetInt("/sys/class/video/disable_video", show ? 0 : 1);
}

void CLinuxC1Codec::SetSpeed(int speed)
{
  CLog::Log(LOGDEBUG, "%s::%s", CLASSNAME, __func__);

  m_speed = speed;

  switch(speed)
  {
    case DVD_PLAYSPEED_PAUSE:
      codec_pause(&am_private->vcodec);
      codec_set_cntl_mode(&am_private->vcodec, TRICKMODE_NONE);
      break;
    case DVD_PLAYSPEED_NORMAL:
      codec_resume(&am_private->vcodec);
      codec_set_cntl_mode(&am_private->vcodec, TRICKMODE_NONE);
      break;
    default:
      codec_resume(&am_private->vcodec);
      if ((am_private->video_format == VFORMAT_H264) || (am_private->video_format == VFORMAT_H264_4K2K))
        codec_set_cntl_mode(&am_private->vcodec, TRICKMODE_FFFB);
      else
        codec_set_cntl_mode(&am_private->vcodec, TRICKMODE_I);
      break;
  }
}

bool CLinuxC1Codec::GetPicture(DVDVideoPicture *pDvdVideoPicture)
{
  debug_log(LOGDEBUG, "%s::%s", CLASSNAME, __func__);

  pDvdVideoPicture->iDuration = (double)(am_private->video_rate * DVD_TIME_BASE) / UNIT_FREQ;

  pDvdVideoPicture->dts = DVD_NOPTS_VALUE;
  if (m_speed == DVD_PLAYSPEED_NORMAL)
  {
    pDvdVideoPicture->pts = GetPlayerPtsSeconds() * (double)DVD_TIME_BASE;
    // video pts cannot be late or dvdplayer goes nuts,
    // so run it one frame ahead
    pDvdVideoPicture->pts += 1 * pDvdVideoPicture->iDuration;
  }
  else
  {
    // We are FF/RW; Do not use the Player clock or it just doesn't work
    if (m_cur_pts == 0)
      pDvdVideoPicture->pts = (double)m_1st_pts / PTS_FREQ * DVD_TIME_BASE;
    else
      pDvdVideoPicture->pts = (double)m_cur_pts / PTS_FREQ * DVD_TIME_BASE;
  }

  return true;
}

void CLinuxC1Codec::CloseDecoder() {
  CLog::Log(LOGDEBUG, "%s::%s", CLASSNAME, __func__);

  // never leave vcodec ff/rw or paused.
  if (m_speed != DVD_PLAYSPEED_NORMAL)
  {
    codec_resume(&am_private->vcodec);
    codec_set_cntl_mode(&am_private->vcodec, TRICKMODE_NONE);
  }
  codec_close(&am_private->vcodec);

  am_packet_release(&am_private->am_pkt);
  free(am_private->extradata);
  am_private->extradata = NULL;
  SysfsUtils::SetInt("/sys/class/tsync/enable", 1);

  SetViewport(0, 0);
  ShowMainVideo(false);
  usleep(500 * 1000);
}

double CLinuxC1Codec::GetPlayerPtsSeconds()
{
  double clock_pts = 0.0;
#ifndef THIS_IS_NOT_XBMC
  CDVDClock *playerclock = CDVDClock::GetMasterClock();
  if (playerclock)
    clock_pts = playerclock->GetClock() / DVD_TIME_BASE;
#endif
  return clock_pts;
}

int CLinuxC1Codec::Decode(uint8_t *pData, size_t iSize, double dts, double pts) {
  debug_log(LOGDEBUG, "%s::%s", CLASSNAME, __func__);

  if (pData)
  {
    am_private->am_pkt.data = pData;
    am_private->am_pkt.data_size = iSize;

    am_private->am_pkt.newflag    = 1;
    am_private->am_pkt.isvalid    = 1;
    am_private->am_pkt.avduration = 0;

    // handle pts, including 31bit wrap, aml can only handle 31
    // bit pts as it uses an int in kernel.
    if (m_hints.ptsinvalid || pts == DVD_NOPTS_VALUE)
      am_private->am_pkt.avpts = AV_NOPTS_VALUE;
    else
    {
      am_private->am_pkt.avpts = 0.5 + (pts * PTS_FREQ) / DVD_TIME_BASE;\
      if (!m_start_pts && am_private->am_pkt.avpts >= 0x7fffffff)
        m_start_pts = am_private->am_pkt.avpts & ~0x0000ffff;
    }
    if (am_private->am_pkt.avpts != (int64_t)AV_NOPTS_VALUE)
      am_private->am_pkt.avpts -= m_start_pts;


    // handle dts, including 31bit wrap, aml can only handle 31
    // bit dts as it uses an int in kernel.
    if (dts == DVD_NOPTS_VALUE)
      am_private->am_pkt.avdts = AV_NOPTS_VALUE;
    else
    {
      am_private->am_pkt.avdts = 0.5 + (dts * PTS_FREQ) / DVD_TIME_BASE;
      if (!m_start_dts && am_private->am_pkt.avdts >= 0x7fffffff)
        m_start_dts = am_private->am_pkt.avdts & ~0x0000ffff;
    }
    if (am_private->am_pkt.avdts != (int64_t)AV_NOPTS_VALUE)
      am_private->am_pkt.avdts -= m_start_dts;

    debug_log(LOGDEBUG, "%s::%s: iSize(%d), dts(%f), pts(%f), avdts(%llx), avpts(%llx)",
      CLASSNAME, __func__, iSize, dts, pts, am_private->am_pkt.avdts, am_private->am_pkt.avpts);

    while (am_private->am_pkt.isvalid)
    {
      // abort on any errors.
      if (write_av_packet(am_private, &am_private->am_pkt) != PLAYER_SUCCESS)
        break;

      if (am_private->am_pkt.isvalid)
        CLog::Log(LOGDEBUG, "%s::%s: write_av_packet looping", CLASSNAME, __func__);
    }

    // if we seek, then GetTimeSize is wrong as
    // reports lastpts - cur_pts and hw decoder has
    // not started outputing new pts values yet.
    // so we grab the 1st pts sent into driver and
    // use that to calc GetTimeSize.
    if (m_1st_pts == 0)
      m_1st_pts = am_private->am_pkt.lastpts;
  }

  int64_t pts_video = 0;
  int rtn = VC_BUFFER;
  pts_video = get_pts_video();
  if (pts_video != m_cur_pts) {
    m_cur_pts = pts_video;
    m_cur_pictcnt++;
    m_old_pictcnt++;
    rtn |= VC_PICTURE;
  }

  debug_log(LOGDEBUG, "%s::%s rtn(%d), m_cur_pictcnt(%lld), m_cur_pts(%f), lastpts(%f)",
    CLASSNAME, __func__, rtn, m_cur_pictcnt, (float)m_cur_pts/PTS_FREQ, (float)am_private->am_pkt.lastpts/PTS_FREQ);

  return rtn;
}

void CLinuxC1Codec::Reset() {
  CLog::Log(LOGDEBUG, "%s::%s", CLASSNAME, __func__);

  int blackout_policy;
  SysfsUtils::GetInt("/sys/class/video/blackout_policy", blackout_policy);
  SysfsUtils::SetInt("/sys/class/video/blackout_policy", 0);

  if (m_speed != DVD_PLAYSPEED_NORMAL)
  {
    codec_resume(&am_private->vcodec);
    codec_set_cntl_mode(&am_private->vcodec, TRICKMODE_NONE);
  }
  codec_reset(&am_private->vcodec);

  am_packet_release(&am_private->am_pkt);
  memzero(am_private->am_pkt);
  am_private->am_pkt.codec = &am_private->vcodec;
  pre_header_feeding(am_private, &am_private->am_pkt);

  SysfsUtils::SetInt("/sys/class/video/blackout_policy", blackout_policy);

  m_1st_pts = 0;
  m_cur_pts = 0;
  m_cur_pictcnt = 0;
  m_old_pictcnt = 0;
  SetSpeed(m_speed);
}

void CLinuxC1Codec::SetViewport(int width, int height) {

  if (m_hints.aspect > 0.0 && (((int)lrint(height * m_hints.aspect)) & -3) > width)
    width = ((int)lrint(height * m_hints.aspect)) & -3;

  char setting[256] = {};
  double scale;
  int displayWidth = CDisplaySettings::Get().GetCurrentResolutionInfo().iWidth;
  int displayHeight = CDisplaySettings::Get().GetCurrentResolutionInfo().iHeight;
  int cutWidth;
  int cutHeight;

  if (width > 0 && height > 0) {
    scale = fmin((double)displayWidth / (double)width, (double)displayHeight / (double)height);
    cutWidth = (displayWidth - (int)((double)width * scale)) / 2;
    cutHeight = (displayHeight - (int)((double)height * scale)) / 2;
    sprintf(setting, "%d %d %d %d", 0 + cutWidth, 0 + cutHeight, displayWidth - cutWidth, displayHeight - cutHeight);
  } else {
    sprintf(setting, "%d %d %d %d", 0, 0, 0, 0);
  }

  CLog::Log(LOGDEBUG, "%s::%s Setting viewport to %s", CLASSNAME, __func__, setting);
  SysfsUtils::SetString("/sys/class/video/axis", setting);
  SysfsUtils::SetInt("/sys/class/video/screen_mode", 1);
}
