#include "common.h"
#include "LinuxV4l2Sink.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CLinuxV4l2Sink"

CLinuxV4l2Sink::CLinuxV4l2Sink(int fd, enum v4l2_buf_type type) {
  Log(LOGDEBUG, "%s::%s - Creating Sink, Device %d, Type %d", CLASSNAME, __func__, fd, type);
  m_Device = fd;
  m_Type = type;
  m_Buffers = NULL;
  m_Planes = NULL;
}

CLinuxV4l2Sink::~CLinuxV4l2Sink() {
  Log(LOGDEBUG, "%s::%s - Destroying Sink, Device %d, Type %d", CLASSNAME, __func__, m_Device, m_Type);

  StreamOn(VIDIOC_STREAMOFF);

  if (m_Memory == V4L2_MEMORY_MMAP)
    for (int i = 0; i < m_NumBuffers*m_NumPlanes; i++)
      if(m_Addresses[i] != (unsigned long)MAP_FAILED)
        if (munmap((void *)m_Addresses[i], m_Planes[i].length) == 0)
          Log(LOGDEBUG, "%s::%s - Munmapped Plane %d size %u at 0x%lx", CLASSNAME, __func__, i, m_Planes[i].length, m_Addresses[i]);

  if (m_Planes)
    delete[] m_Planes;
  if (m_Buffers)
    delete[] m_Buffers;
  if (m_Addresses)
    delete[] m_Addresses;
}

// Init for MMAP buffers
bool CLinuxV4l2Sink::Init(v4l2_format *format, int buffersCount = 0) {
  Log(LOGDEBUG, "%s::%s - Init MMAP %d buffers", CLASSNAME, __func__, buffersCount);
  m_Memory = V4L2_MEMORY_MMAP;
  SetFormat(format);
  GetFormat(format);

  if (buffersCount == 0 && m_Type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    if (ioctl(m_Device, VIDIOC_G_CTRL, &ctrl))
      return false;
    buffersCount = (int)(ctrl.value * 1.5); //Most of the time we need 50% more extra capture buffers than device reported would be enough
  }

  m_NumBuffers = RequestBuffers(buffersCount);
  if (m_NumBuffers < 1)
    return false;
  m_Buffers = new v4l2_buffer[m_NumBuffers];
  m_Planes = new v4l2_plane[m_NumPlanes * m_NumBuffers];
  m_Addresses = new unsigned long[m_NumPlanes * m_NumBuffers];
  QueryBuffers();
  MmapBuffers();
  return true;
}
// Init for USERPTR buffers
bool CLinuxV4l2Sink::Init(v4l2_format *format, CLinuxV4l2Sink *sink) {
  Log(LOGDEBUG, "%s::%s - Init UserPTR", CLASSNAME, __func__);
  m_Memory = V4L2_MEMORY_USERPTR;
  SetFormat(format);
  GetFormat(format);

  m_NumBuffers = sink->m_NumBuffers;
  m_NumBuffers = RequestBuffers(m_NumBuffers);
  if (m_NumBuffers < 1)
    return false;
  m_Buffers = new v4l2_buffer[m_NumBuffers];
  m_Planes = new v4l2_plane[m_NumPlanes * m_NumBuffers];
  m_Addresses = new unsigned long[m_NumPlanes * m_NumBuffers];
  QueryBuffers();
  for (int i = 0; i < m_NumPlanes * m_NumBuffers; i++) {
    m_Addresses[i] = sink->m_Addresses[i];
    m_Planes[i].m.userptr = m_Addresses[i];
  }
  return true;
}

bool CLinuxV4l2Sink::GetFormat(v4l2_format *format) {
  memset(format, 0, sizeof(struct v4l2_format));
  format->type = m_Type;
  if (ioctl(m_Device, VIDIOC_G_FMT, format))
    return false;
  m_NumPlanes = format->fmt.pix_mp.num_planes;
  Log(LOGDEBUG, "%s::%s - G_FMT Device %d, Type %d format 0x%x (%dx%d), plane[0]=%d plane[1]=%d", CLASSNAME, __func__, m_Device, format->type, format->fmt.pix_mp.pixelformat, format->fmt.pix_mp.width, format->fmt.pix_mp.height, format->fmt.pix_mp.plane_fmt[0].sizeimage, format->fmt.pix_mp.plane_fmt[1].sizeimage);
  return true;
}

bool CLinuxV4l2Sink::SetFormat(v4l2_format *format) {
  format->type = m_Type;
  if (format->fmt.pix_mp.pixelformat == 0)
    return false;
  Log(LOGDEBUG, "%s::%s - S_FMT Device %d, Type %d format 0x%x buffer size=%u", CLASSNAME, __func__, m_Device, format->type, format->fmt.pix_mp.pixelformat, format->fmt.pix_mp.plane_fmt[0].sizeimage);
  format->type = m_Type;
  if (ioctl(m_Device, VIDIOC_S_FMT, format))
    return false;
  return true;
}

bool CLinuxV4l2Sink::GetCrop(v4l2_crop *crop) {
  memset(crop, 0, sizeof(struct v4l2_crop));
  crop->type = m_Type;
  if (ioctl(m_Device, VIDIOC_G_CROP, crop))
    return false;
  Log(LOGDEBUG, "%s::%s - G_CROP Device %d, Type %d, crop (%dx%d)", CLASSNAME, __func__, m_Device, crop->type, crop->c.width, crop->c.height);
  return true;
}

bool CLinuxV4l2Sink::SetCrop(v4l2_crop *crop) {
  crop->type = m_Type;
  Log(LOGDEBUG, "%s::%s - S_CROP Device %d, Type %d, crop (%dx%d)", CLASSNAME, __func__, m_Device, crop->type, crop->c.width, crop->c.height);
  if (ioctl(m_Device, VIDIOC_S_CROP, crop))
    return false;
  return true;
}

int CLinuxV4l2Sink::RequestBuffers(int buffersCount) {
  Log(LOGDEBUG, "%s::%s - Device %d, Type %d, RequestBuffers %d", CLASSNAME, __func__, m_Device, m_Type, buffersCount);
  struct v4l2_requestbuffers reqbuf;
  memset(&reqbuf, 0, sizeof(struct v4l2_requestbuffers));
  reqbuf.type     = m_Type;
  reqbuf.memory   = m_Memory;
  reqbuf.count    = buffersCount;

  if (ioctl(m_Device, VIDIOC_REQBUFS, &reqbuf))
    return V4L2_ERROR;

  Log(LOGDEBUG, "%s::%s - Device %d, Type %d, Buffers allowed %d", CLASSNAME, __func__, m_Device, m_Type, reqbuf.count);
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

    if (ioctl(m_Device, VIDIOC_QUERYBUF, &m_Buffers[i]))
      return false;

    iFreeBuffers.push(m_Buffers[i].index);
  }
  return true;
}

bool CLinuxV4l2Sink::MmapBuffers() {
  for(int i = 0; i < m_NumBuffers * m_NumPlanes; i++) {
    if(m_Planes[i].length) {
      m_Addresses[i] = (unsigned long)mmap(NULL, m_Planes[i].length, PROT_READ | PROT_WRITE, MAP_SHARED, m_Device, m_Planes[i].m.mem_offset);
      if (m_Addresses[i] == (unsigned long)MAP_FAILED)
        return false;
      Log(LOGDEBUG, "%s::%s - MMapped Plane %d at 0x%x in device to address 0x%lx", CLASSNAME, __func__, i, m_Planes[i].m.mem_offset, m_Addresses[i]);
    }
  }
  return true;
}

bool CLinuxV4l2Sink::StreamOn(int state) {
  if(ioctl(m_Device, state, &m_Type))
    return false;
  Log(LOGDEBUG, "%s::%s - %d", CLASSNAME, __func__, state);
  return true;
}

bool CLinuxV4l2Sink::QueueBuffer(v4l2_buffer *buffer) {
  Log(LOGDEBUG, "%s::%s - Device %d, Memory %d, Type %d <- %d", CLASSNAME, __func__, m_Device, buffer->memory, buffer->type, buffer->index);
  if (ioctl(m_Device, VIDIOC_QBUF, buffer))
    return false;
  return true;
}
bool CLinuxV4l2Sink::DequeueBuffer(v4l2_buffer *buffer) {
  if (ioctl(m_Device, VIDIOC_DQBUF, buffer))
    return false;
  Log(LOGDEBUG, "%s::%s - Device %d, Memory %d, Type %d -> %d", CLASSNAME, __func__, m_Device, buffer->memory, buffer->type, buffer->index);
  return true;
}

bool CLinuxV4l2Sink::GetBuffer(V4l2SinkBuffer *buffer) {
  int bufIndex;

  if (iFreeBuffers.empty()) {
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
    iFreeBuffers.push(buf.index);
  }

  buffer->iIndex = iFreeBuffers.front();
  iFreeBuffers.pop();
  for (int i = 0; i < m_NumPlanes; i++)
    buffer->cPlane[i] = (void *)m_Addresses[buffer->iIndex * m_NumPlanes + i];
  return true;
}

bool CLinuxV4l2Sink::PushBuffer(V4l2SinkBuffer *buffer) {
  if (m_Memory == V4L2_MEMORY_USERPTR)
    for (int i = 0; i < m_NumPlanes; i++)
      m_Buffers[buffer->iIndex].m.planes[i].m.userptr = (long unsigned int)buffer->cPlane[i];

  for (int i = 0; i < m_NumPlanes; i++)
    m_Buffers[buffer->iIndex].m.planes[i].bytesused = buffer->iBytesUsed[i];
  if (!QueueBuffer(&m_Buffers[buffer->iIndex]))
    return false;
  return true;
}

int CLinuxV4l2Sink::Poll(int timeout) {
  struct pollfd p;
  p.fd = m_Device;
  if (m_Type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
    p.events = POLLOUT | POLLERR;
  else
    p.events = POLLIN | POLLERR;

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
