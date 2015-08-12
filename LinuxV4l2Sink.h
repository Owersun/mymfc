#pragma once

#include <queue>
#include <string>
#include <poll.h>
#include <linux/videodev2.h>

#ifndef V4L2_BUF_FLAG_TIMESTAMP_COPY
  #define V4L2_BUF_FLAG_TIMESTAMP_COPY 0x4000
#endif

#define V4L2_ERROR -1
#define V4L2_BUSY  1
#define V4L2_READY 2
#define V4L2_OK    3

#ifdef _DEBUG
  #define debug_log(...) CLog::Log(__VA_ARGS__)
#else
  #define debug_log(...)
#endif

typedef struct V4l2Device
{
  int     device;
  char    name[32];
} V4l2Device;

typedef struct V4l2SinkBuffer
{
  int     iIndex;
  int     iBytesUsed[4];
  void    *cPlane[4];
  struct  timeval timeStamp;
} V4l2SinkBuffer;

class CLinuxV4l2Sink
{
public:
  CLinuxV4l2Sink(V4l2Device *device, enum v4l2_buf_type type);
  ~CLinuxV4l2Sink();

  bool Init(int buffersCount);
  bool Init(CLinuxV4l2Sink *sink);
  void SoftRestart();
  bool GetFormat(v4l2_format *format);
  bool SetFormat(v4l2_format *format);
  bool GetCrop(v4l2_crop *crop);
  bool SetCrop(v4l2_crop *crop);
  bool GetBuffer(V4l2SinkBuffer* buffer);
  bool DequeueBuffer(V4l2SinkBuffer* buffer);
  bool PushBuffer(V4l2SinkBuffer* buffer);
  bool StreamOn(int state);
  bool QueueAll();
  int Poll(int timeout);
private:
  V4l2Device *m_Device;
  int m_NumPlanes;
  int m_NumBuffers;
  std::queue<int> iFreeBuffers;
  enum v4l2_memory m_Memory;
  enum v4l2_buf_type m_Type;
  v4l2_buffer *m_Buffers;
  v4l2_plane *m_Planes;
  unsigned long *m_Addresses;
  int RequestBuffers(int buffersCount);
  bool QueryBuffers();
  bool MmapBuffers();
  bool QueueBuffer(v4l2_buffer *buffer);
  bool DequeueBuffer(v4l2_buffer *buffer);
};
