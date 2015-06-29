#pragma once

#include "parser.h"
#include "DVDVideoCodecMFC.h"

CDVDVideoCodecMFC* m_cVideoCodec;
DVDVideoPicture* m_pDvdVideoPicture;
Parser* parser;
CDVDStreamInfo* m_cHints;
unsigned char* m_cFrameData;

typedef struct inputData {
  char *name;
  int fd;
  char *p;
  int size;
  int offs;
} inputData;

