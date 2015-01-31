#pragma once

#include "parser.h"
#include "DVDVideoCodecMFC.h"

CDVDVideoCodecMFC* m_cVideoCodec;
Parser* parser;
CDVDStreamInfo* m_cHints;
char* m_cFrameData;

typedef struct inputData {
  char *name;
  int fd;
  char *p;
  int size;
  int offs;
} inputData;

#define BUFFER_SIZE        1048576 //compressed frame size. 1080p mpeg4 10Mb/s can be >256k in size, so this is to make sure frame fits into buffer
                                          //for very unknown reason lesser than 1Mb buffer causes MFC to corrupt its own setup setting inapropriate values
#define INPUT_BUFFERS      3       //triple buffering for smooth DRM output
#define OUTPUT_BUFFERS     3       //3 input buffers. 2 is enough almost for everything, but on some heavy videos 3 makes a difference
