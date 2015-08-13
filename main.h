#pragma once

#include "DVDVideoCodecC1.h"

CDVDVideoCodecC1* m_cVideoCodec;
DVDVideoPicture* m_pDvdVideoPicture;
CDVDStreamInfo* m_cHints;
AVFormatContext* formatCtx;
AVCodecContext* codecCtx;
AVCodec* codec;
