V4l2Device *m_iDecoderHandle;
V4l2Device *m_iConverterHandle;

CLinuxV4l2Sink *iMFCCapture;
CLinuxV4l2Sink *iMFCOutput;
CLinuxV4l2Sink *iFIMCCapture;
CLinuxV4l2Sink *iFIMCOutput;

struct in {
  char *name;
  int fd;
  char *p;
  int size;
  int offs;
};

#define BUFFER_SIZE        1048576 //compressed frame size. 1080p mpeg4 10Mb/s can be >256k in size, so this is to make sure frame fits into buffer
                                          //for very unknown reason lesser than 1Mb buffer causes MFC to corrupt its own setup setting inapropriate values
#define INPUT_BUFFERS      3       //triple buffering for smooth DRM output
#define OUTPUT_BUFFERS     3       //3 input buffers. 2 is enough almost for everything, but on some heavy videos 3 makes a difference
