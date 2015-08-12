#pragma once

#include "DVDVideoCodecMFC.h"

CDVDVideoCodecMFC* m_cVideoCodec;
DVDVideoPicture* m_pDvdVideoPicture;
CDVDStreamInfo* m_cHints;
AVFormatContext* formatCtx;
AVCodecContext* codecCtx;
AVCodec* codec;
