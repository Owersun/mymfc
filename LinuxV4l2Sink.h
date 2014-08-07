#pragma once

/*
 *      Copyright (C) 2005-2012 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <queue>
#include <linux/videodev2.h>

#ifndef V4L2_BUF_FLAG_TIMESTAMP_COPY
  #define V4L2_BUF_FLAG_TIMESTAMP_COPY 0x4000
#endif

#define V4L2_ERROR -1
#define V4L2_BUSY  1
#define V4L2_READY 2
#define V4L2_OK    3

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

typedef struct V4l2SinkBuffer
{
  int   iIndex;
  int   iBytesUsed[4];
  void  *cPlane[4];
  double iTimestamp;
} V4l2SinkBuffer;

class CLinuxV4l2Sink
{
public:
  CLinuxV4l2Sink(int fd, enum v4l2_buf_type type);
  ~CLinuxV4l2Sink();

  bool Init(v4l2_format *format, int buffersCount);
  bool Init(v4l2_format *format, CLinuxV4l2Sink *sink);
  bool GetFormat(v4l2_format *format);
  bool SetFormat(v4l2_format *format);
  bool GetCrop(v4l2_crop *crop);
  bool SetCrop(v4l2_crop *crop);
  bool GetBuffer(V4l2SinkBuffer* buffer);
  bool PushBuffer(V4l2SinkBuffer* buffer);
  bool StreamOn(int state);
  bool QueueAll();
private:
  int m_Device;
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
