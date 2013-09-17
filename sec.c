 
  typedef struct {
      unsigned int  len;
      void *start;
  } qbuf_t;

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/poll.h>

int cam_fd;
qbuf_t qbuf;

int main()
{   
    struct v4l2_requestbuffers req = {0};
    unsigned int loop;
    int  rtn, qcnt;
    struct pollfd poll_fds;
    struct v4l2_buffer buf = {0};
    int tmout = 1000;

    cam_fd = open("/dev/video0", O_RDWR|O_NONBLOCK, 0);
    printf("fd:%d\n", cam_fd);
    if (cam_fd == -1)
    {
            perror("Opening video device");
            return 1;
    }

    qcnt = 1;
    
    //CLEAR_STRUCT(req);
    req.count  = qcnt;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    rtn = ioctl( cam_fd, VIDIOC_REQBUFS, &req );
    if ( 0 > rtn )
    {
        perror("VIDIOC_REQBUFS" );
        return -1;
    }
    
    printf( " qcount=%d(app) req.count=%d(dev)\n", qcnt, req.count );   
  
    for(loop=0; loop<req.count; loop++)
    {
        struct v4l2_buffer buf = {0};
        
        //CLEAR_STRUCT(buf);
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = loop;
  
        rtn = ioctl( cam_fd, VIDIOC_QUERYBUF, &buf );
        if ( 0 > rtn )
        {
            perror("VIDIOC_QUERYBUF" );
            return -1;
        }
        
        qbuf.len   = buf.length;
        qbuf.start = mmap (NULL,
                            buf.length,
                            PROT_READ | PROT_WRITE, /* required */
                            MAP_SHARED,             /* recommended */
                            cam_fd, buf.m.offset );  
                           
        if ( MAP_FAILED == qbuf.start )
        {
            perror("mmap failed" ); 
            return -1;
        }
  
        printf(" %d) start =0x%p\n", loop, qbuf.start );
        printf("    length=0x%05x (%d)\n", buf.length, buf.length   );
        
        //qbuf++;
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(cam_fd, VIDIOC_STREAMON, &type);
    
    
    rtn = poll( (struct pollfd *)&poll_fds, 1, tmout );
    if ( rtn < 0 ) return -1;
    
    {
    //CLEAR_STRUCT(buf);
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
  
    rtn = ioctl( cam_fd, VIDIOC_DQBUF, &buf );  
    if ( rtn == -1 ) rtn = ioctl( cam_fd, VIDIOC_QBUF, &buf );
    }

    printf("buf.bytesused:%d\n", buf.bytesused);
    
    printf("Close()\n");
    close(cam_fd);
    
    return 0;
}