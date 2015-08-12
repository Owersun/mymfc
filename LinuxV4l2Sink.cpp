#include "system.h"

#ifndef THIS_IS_NOT_XBMC
  #if (defined HAVE_CONFIG_H) && (!defined WIN32)
    #include "config.h"
  #endif

  #include "utils/log.h"
#endif

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <linux/media.h>

#include "LinuxV4l2Sink.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CLinuxV4l2Sink"

CLinuxV4l2Sink::CLinuxV4l2Sink(V4l2Device *device, enum v4l2_buf_type type) {
  CLog::Log(LOGDEBUG, "%s::%s - Creating Sink, Device %s, Type %d", CLASSNAME, __func__, device->name, type);
  m_Device = device;
  m_Type = type;
  m_NumBuffers = 0;
  m_NumPlanes = 0;
  m_Addresses = NULL;
  m_Buffers = NULL;
  m_Planes = NULL;
}

CLinuxV4l2Sink::~CLinuxV4l2Sink() {
  CLog::Log(LOGDEBUG, "%s::%s - Destroying Sink, Device %s, Type %d", CLASSNAME, __func__, m_Device->name, m_Type);

  StreamOn(VIDIOC_STREAMOFF);

  if (m_Memory == V4L2_MEMORY_MMAP)
    for (int i = 0; i < m_NumBuffers*m_NumPlanes; i++)
      if(m_Addresses[i] != (unsigned long)MAP_FAILED)
        if (munmap((void *)m_Addresses[i], m_Planes[i].length) == 0)
          CLog::Log(LOGDEBUG, "%s::%s - Device %s, Munmapped Plane %d size %u at 0x%lx", CLASSNAME, __func__, m_Device->name, i, m_Planes[i].length, m_Addresses[i]);
  if (m_Planes)
    delete[] m_Planes;
  if (m_Buffers)
    delete[] m_Buffers;
  if (m_Addresses)
    delete[] m_Addresses;
}

// Init for MMAP buffers
bool CLinuxV4l2Sink::Init(int buffersCount = 0) {
  CLog::Log(LOGDEBUG, "%s::%s - Device %s, Type %d, Init MMAP %d buffers", CLASSNAME, __func__, m_Device->name, m_Type, buffersCount);
  m_Memory = V4L2_MEMORY_MMAP;

  struct v4l2_format format;
  if (!GetFormat(&format))
    return false;

  if (buffersCount == 0 && m_Type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    if (ioctl(m_Device->device, VIDIOC_G_CTRL, &ctrl)) {
      CLog::Log(LOGERROR, "%s::%s - Device %s, Type %d, Error getting number of buffers for capture (V4L2_CID_MIN_BUFFERS_FOR_CAPTURE VIDIOC_G_CTRL)", CLASSNAME, __func__, m_Device->name, m_Type);
      return false;
    }
    buffersCount = (int)(ctrl.value * 1.5); //Most of the time we need 50% more extra capture buffers than device reported would be enough
  }

  m_NumBuffers = RequestBuffers(buffersCount);
  if (m_NumBuffers < 1)
    return false;
  m_Buffers = new v4l2_buffer[m_NumBuffers];
  m_Planes = new v4l2_plane[m_NumPlanes * m_NumBuffers];
  m_Addresses = new unsigned long[m_NumPlanes * m_NumBuffers];
  if (!QueryBuffers())
    return false;
  if (!MmapBuffers())
    return false;
  return true;
}
// Init for USERPTR buffers
bool CLinuxV4l2Sink::Init(CLinuxV4l2Sink *sink) {
  CLog::Log(LOGDEBUG, "%s::%s - Device %s, Type %d, Init UserPTR", CLASSNAME, __func__, m_Device->name, m_Type);
  m_Memory = V4L2_MEMORY_USERPTR;

  struct v4l2_format format;
  if (!GetFormat(&format))
    return false;

  m_NumBuffers = sink->m_NumBuffers;
  m_NumBuffers = RequestBuffers(m_NumBuffers);
  if (m_NumBuffers < 1)
    return false;
  m_Buffers = new v4l2_buffer[m_NumBuffers];
  m_Planes = new v4l2_plane[m_NumPlanes * m_NumBuffers];
  m_Addresses = new unsigned long[m_NumPlanes * m_NumBuffers];
  if (!QueryBuffers())
    return false;
  for (int i = 0; i < m_NumPlanes * m_NumBuffers; i++) {
    m_Addresses[i] = sink->m_Addresses[i];
    m_Planes[i].m.userptr = m_Addresses[i];
  }
  return true;
}

void CLinuxV4l2Sink::SoftRestart() {
  StreamOn(VIDIOC_STREAMOFF);

  while (!iFreeBuffers.empty())
    iFreeBuffers.pop();
  for (int i = 0; i < m_NumBuffers; i++)
    iFreeBuffers.push(m_Buffers[i].index);

  if (m_Type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    QueueAll();

  StreamOn(VIDIOC_STREAMON);
}

bool CLinuxV4l2Sink::GetFormat(v4l2_format *format) {
  memset(format, 0, sizeof(struct v4l2_format));
  format->type = m_Type;
  if (ioctl(m_Device->device, VIDIOC_G_FMT, format)) {
    CLog::Log(LOGERROR, "%s::%s - Error getting sink format. Device %s, Type %d. (VIDIOC_G_FMT)", CLASSNAME, __func__, m_Device->name, m_Type);
    return false;
  }
  m_NumPlanes = format->fmt.pix_mp.num_planes;
  CLog::Log(LOGDEBUG, "%s::%s - G_FMT Device %s, Type %d format 0x%x (%dx%d), planes=%d, plane[0]=%d plane[1]=%d, plane[2]=%d", CLASSNAME, __func__, m_Device->name, format->type, format->fmt.pix_mp.pixelformat, format->fmt.pix_mp.width, format->fmt.pix_mp.height, format->fmt.pix_mp.num_planes, format->fmt.pix_mp.plane_fmt[0].sizeimage, format->fmt.pix_mp.plane_fmt[1].sizeimage, format->fmt.pix_mp.plane_fmt[2].sizeimage);
  return true;
}

bool CLinuxV4l2Sink::SetFormat(v4l2_format *format) {
  format->type = m_Type;
  CLog::Log(LOGDEBUG, "%s::%s - S_FMT Device %s, Type %d format 0x%x (%dx%d), planes=%d, plane[0]=%d plane[1]=%d, plane[2]=%d", CLASSNAME, __func__, m_Device->name, format->type, format->fmt.pix_mp.pixelformat, format->fmt.pix_mp.width, format->fmt.pix_mp.height, format->fmt.pix_mp.num_planes, format->fmt.pix_mp.plane_fmt[0].sizeimage, format->fmt.pix_mp.plane_fmt[1].sizeimage, format->fmt.pix_mp.plane_fmt[2].sizeimage);
  if (ioctl(m_Device->device, VIDIOC_S_FMT, format)) {
    CLog::Log(LOGERROR, "%s::%s - Error setting sink format. Device %s, Type %d. (VIDIOC_G_FMT)", CLASSNAME, __func__, m_Device->name, m_Type);
    return false;
  }
  return true;
}

bool CLinuxV4l2Sink::GetCrop(v4l2_crop *crop) {
  memset(crop, 0, sizeof(struct v4l2_crop));
  crop->type = m_Type;
  if (ioctl(m_Device->device, VIDIOC_G_CROP, crop)) {
    CLog::Log(LOGERROR, "%s::%s - Error getting sink crop. Device %s, Type %d. (VIDIOC_G_CROP)", CLASSNAME, __func__, m_Device->name, m_Type);
    return false;
  }
  CLog::Log(LOGDEBUG, "%s::%s - G_CROP Device %s, Type %d, crop (%dx%d)", CLASSNAME, __func__, m_Device->name, crop->type, crop->c.width, crop->c.height);
  return true;
}

bool CLinuxV4l2Sink::SetCrop(v4l2_crop *crop) {
  crop->type = m_Type;
  CLog::Log(LOGDEBUG, "%s::%s - S_CROP Device %s, Type %d, crop (%dx%d)", CLASSNAME, __func__, m_Device->name, crop->type, crop->c.width, crop->c.height);
  if (ioctl(m_Device->device, VIDIOC_S_CROP, crop)) {
    CLog::Log(LOGERROR, "%s::%s - Error setting sink crop. Device %s, Type %d. (VIDIOC_G_CROP)", CLASSNAME, __func__, m_Device->name, m_Type);
    return false;
  }
  return true;
}

int CLinuxV4l2Sink::RequestBuffers(int buffersCount) {
  CLog::Log(LOGDEBUG, "%s::%s - Device %s, Type %d, Memory %d, RequestBuffers %d", CLASSNAME, __func__, m_Device->name, m_Type, m_Memory, buffersCount);
  struct v4l2_requestbuffers reqbuf;
  memset(&reqbuf, 0, sizeof(struct v4l2_requestbuffers));
  reqbuf.type     = m_Type;
  reqbuf.memory   = m_Memory;
  reqbuf.count    = buffersCount;

  if (ioctl(m_Device->device, VIDIOC_REQBUFS, &reqbuf)) {
    CLog::Log(LOGERROR, "%s::%s - Error requesting buffers. Device %s, Type %d, Memory %d. (VIDIOC_REQBUFS)", CLASSNAME, __func__, m_Device->name, m_Type, m_Memory);
    return V4L2_ERROR;
  }

  CLog::Log(LOGDEBUG, "%s::%s - Device %s, Type %d, Memory %d, Buffers allowed %d", CLASSNAME, __func__, m_Device->name, m_Type, m_Memory, reqbuf.count);
  return reqbuf.count;
}

bool CLinuxV4l2Sink::QueryBuffers() {
  memset(m_Buffers, 0, m_NumBuffers * sizeof(struct v4l2_buffer));
  memset(m_Planes, 0, m_NumBuffers * m_NumPlanes * sizeof(struct v4l2_plane));

  for(int i = 0; i < m_NumBuffers; i++) {
    m_Buffers[i].type      = m_Type;
    m_Buffers[i].memory    = m_Memory;
    m_Buffers[i].index     = i;
    m_Buffers[i].m.planes  = &m_Planes[i*m_NumPlanes];
    m_Buffers[i].length    = m_NumPlanes;

    if (ioctl(m_Device->device, VIDIOC_QUERYBUF, &m_Buffers[i])) {
      CLog::Log(LOGERROR, "%s::%s - Error querying buffers. Device %s, Type %d, Memory %d. (VIDIOC_QUERYBUF)", CLASSNAME, __func__, m_Device->name, m_Type, m_Memory);
      return false;
    }

    iFreeBuffers.push(m_Buffers[i].index);
  }
  return true;
}

bool CLinuxV4l2Sink::MmapBuffers() {
  for(int i = 0; i < m_NumBuffers * m_NumPlanes; i++) {
    if(m_Planes[i].length) {
      m_Addresses[i] = (unsigned long)mmap(NULL, m_Planes[i].length, PROT_READ | PROT_WRITE, MAP_SHARED, m_Device->device, m_Planes[i].m.mem_offset);
      if (m_Addresses[i] == (unsigned long)MAP_FAILED)
        return false;
      CLog::Log(LOGDEBUG, "%s::%s - Device %s, Type %d, MMapped Plane %d at 0x%x to address 0x%lx", CLASSNAME, __func__, m_Device->name, m_Type, i, m_Planes[i].m.mem_offset, m_Addresses[i]);
    }
  }
  return true;
}

bool CLinuxV4l2Sink::StreamOn(int state) {
  if(ioctl(m_Device->device, state, &m_Type)) {
    CLog::Log(LOGERROR, "%s::%s - Error setting device state to %d, Device %s, Type %d.", CLASSNAME, __func__, state, m_Device->name, m_Type);
    return false;
  }
  CLog::Log(LOGDEBUG, "%s::%s - Device %s, Type %d, %d", CLASSNAME, __func__, m_Device->name, m_Type, state);
  return true;
}

bool CLinuxV4l2Sink::QueueBuffer(v4l2_buffer *buffer) {
  debug_log(LOGDEBUG, "%s::%s - Device %s, Type %d, Memory %d <- %d", CLASSNAME, __func__, m_Device->name, buffer->type, buffer->memory, buffer->index);
  if (ioctl(m_Device->device, VIDIOC_QBUF, buffer)) {
    CLog::Log(LOGERROR, "%s::%s - Error queueing buffer. Device %s, Type %d, Memory %d. Buffer %d, errno %d", CLASSNAME, __func__, m_Device->name, buffer->type, buffer->memory, buffer->index, errno);
    return false;
  }
  return true;
}
bool CLinuxV4l2Sink::DequeueBuffer(v4l2_buffer *buffer) {
  if (ioctl(m_Device->device, VIDIOC_DQBUF, buffer)) {
    if (errno != EAGAIN) CLog::Log(LOGERROR, "%s::%s - Error dequeueing buffer. Device %s, Type %d, Memory %d. Buffer %d, errno %d", CLASSNAME, __func__, m_Device->name, buffer->type, buffer->memory, buffer->index, errno);
    return false;
  }
  debug_log(LOGDEBUG, "%s::%s - Device %s, Type %d, Memory %d -> %d", CLASSNAME, __func__, m_Device->name, buffer->type, buffer->memory, buffer->index);
  return true;
}

bool CLinuxV4l2Sink::DequeueBuffer(V4l2SinkBuffer *buffer) {
  struct v4l2_buffer buf;
  struct v4l2_plane  planes[m_NumPlanes];
  memset(&planes, 0, sizeof(struct v4l2_plane) * m_NumPlanes);
  memset(&buf, 0, sizeof(struct v4l2_buffer));
  buf.type     = m_Type;
  buf.memory   = m_Memory;
  buf.m.planes = planes;
  buf.length   = m_NumPlanes;
  if (!DequeueBuffer(&buf))
    return false;

  buffer->iIndex = buf.index;
  buffer->timeStamp = buf.timestamp;
  for (int i = 0; i < m_NumPlanes; i++)
    buffer->cPlane[i] = (void *)m_Addresses[buffer->iIndex * m_NumPlanes + i];
  return true;
}

bool CLinuxV4l2Sink::GetBuffer(V4l2SinkBuffer *buffer) {
  if (iFreeBuffers.empty()) {
    if (!DequeueBuffer(buffer))
      return false;
  } else {
    buffer->iIndex = iFreeBuffers.front();
    buffer->timeStamp = m_Buffers[buffer->iIndex].timestamp;
    iFreeBuffers.pop();
    for (int i = 0; i < m_NumPlanes; i++)
      buffer->cPlane[i] = (void *)m_Addresses[buffer->iIndex * m_NumPlanes + i];
  }
  return true;
}

bool CLinuxV4l2Sink::PushBuffer(V4l2SinkBuffer *buffer) {
  if (m_Memory == V4L2_MEMORY_USERPTR)
    for (int i = 0; i < m_NumPlanes; i++)
      m_Buffers[buffer->iIndex].m.planes[i].m.userptr = (long unsigned int)buffer->cPlane[i];

  if (m_Type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    m_Buffers[buffer->iIndex].timestamp = buffer->timeStamp;
    m_Buffers[buffer->iIndex].flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
    for (int i = 0; i < m_NumPlanes; i++)
      m_Buffers[buffer->iIndex].m.planes[i].bytesused = buffer->iBytesUsed[i];
  }

  if (!QueueBuffer(&m_Buffers[buffer->iIndex]))
    return false;
  return true;
}

int CLinuxV4l2Sink::Poll(int timeout) {
  struct pollfd p;
  p.fd = m_Device->device;
  p.events = POLLERR;
  (m_Type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) ? p.events |= POLLOUT : p.events |= POLLIN;

  return poll(&p, 1, timeout);
}

bool CLinuxV4l2Sink::QueueAll() {
  while (!iFreeBuffers.empty()) {
    if (!QueueBuffer(&m_Buffers[iFreeBuffers.front()]))
      return false;
    iFreeBuffers.pop();
  }
  return true;
}
