#pragma once

#include "parser.h"
#include "DVDVideoCodecC1.h"
#include "LinuxC1Codec.h"

#define BUFFER_SIZE 1048576

CDVDVideoCodecC1* m_cVideoCodec;
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
