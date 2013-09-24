#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <jpeglib.h>
#include <errno.h>



uint8_t *buffer;
struct {
  void *start;
  size_t length;
} *buffers;

struct v4l2_requestbuffers req;
    

static int xioctl(int fd, int request, void *arg)
{
        int r;

        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);

        return r;
}

/**
  Convert from YUV422 format to RGB888. Formulae are described on http://en.wikipedia.org/wiki/YUV

  \param width width of image
  \param height height of image
  \param src source
  \param dst destination
*/
static void YUV422toRGB888(int width, int height, unsigned char *src, unsigned char *dst)
{
  int line, column;
  unsigned char *py, *pu, *pv;
  unsigned char *tmp = dst;

  /* In this format each four bytes is two pixels. Each four bytes is two Y's, a Cb and a Cr. 
     Each Y goes to one of the pixels, and the Cb and Cr belong to both pixels. */
  py = src;
  pu = src + 1;
  pv = src + 3;

  #define CLIP(x) ( (x)>=0xFF ? 0xFF : ( (x) <= 0x00 ? 0x00 : (x) ) )

  for (line = 0; line < height; ++line) {
    for (column = 0; column < width; ++column) {
      *tmp++ = CLIP((double)*py + 1.402*((double)*pv-128.0));
      *tmp++ = CLIP((double)*py - 0.344*((double)*pu-128.0) - 0.714*((double)*pv-128.0));      
      *tmp++ = CLIP((double)*py + 1.772*((double)*pu-128.0));

      // increase py every time
      py += 2;
      // increase pu,pv every second time
      if ((column & 1)==1) {
        pu += 4;
        pv += 4;
      }
    }
  }
}



int print_caps(int fd)
{
        struct v4l2_capability caps = {};
        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps))
        {
                perror("Querying Capabilities\n");
                return 1;
        }

        printf( "Driver Caps:\n"
                "  Driver: \"%s\"\n"
                "  Card: \"%s\"\n"
                "  Bus: \"%s\"\n"
                "  Version: %d.%d\n"
                "  Capabilities: %08x\n",
                caps.driver,
                caps.card,
                caps.bus_info,
                (caps.version>>16)&&0xff,
                (caps.version>>24)&&0xff,
                caps.capabilities);


        struct v4l2_cropcap cropcap = {0};
        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl (fd, VIDIOC_CROPCAP, &cropcap))
        {
                perror("Querying Cropping Capabilities\n");
                return 1;
        }

        printf( "Camera Cropping:\n"
                "  Bounds: %dx%d+%d+%d\n"
                "  Default: %dx%d+%d+%d\n"
                "  Aspect: %d/%d\n",
                cropcap.bounds.width, cropcap.bounds.height, cropcap.bounds.left, cropcap.bounds.top,
                cropcap.defrect.width, cropcap.defrect.height, cropcap.defrect.left, cropcap.defrect.top,
                cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);
        /*
        int support_grbg10 = 0;

        struct v4l2_fmtdesc fmtdesc = {0};
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        */
        char fourcc[5] = {0};
        /*
        char c, e;
        printf("  FMT : CE Desc\n--------------------\n");
        while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
        {
                printf("VIDIOC_ENUM_FMT.\n");
                strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
                printf("pixelformat:%x\n", fmtdesc.pixelformat);
                if (fmtdesc.pixelformat == V4L2_PIX_FMT_SGRBG10)
                    support_grbg10 = 1;
                c = fmtdesc.flags & 1? 'C' : ' ';
                e = fmtdesc.flags & 2? 'E' : ' ';
                printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
                fmtdesc.index++;
        }

        if (!support_grbg10)
        {
            printf("Doesn't support GRBG10.\n");
            return 1;
        }
        */
        struct v4l2_format fmt = {0};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = 640;
        fmt.fmt.pix.height = 480;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SGRBG10;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
        {
            perror("Setting Pixel Format");
            return 1;
        }

        strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
        printf( "Selected Camera Mode:\n"
                "  Width: %d\n"
                "  Height: %d\n"
                "  PixFmt: %s\n"
                "  Field: %d\n",
                fmt.fmt.pix.width,
                fmt.fmt.pix.height,
                fourcc,
                fmt.fmt.pix.field);
        return 0;
}

int init_mmap(int fd)
{      
    unsigned int i;
    
    memset(&req, 0, sizeof(req));
    req.count = 20;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (errno == EINVAL)
          printf("Video capturing or mmap-streaming is not suppported.\n");
        else
          perror("Requesting Buffer");
        return 1;
    }
    
    if (req.count < 5) {
      printf("Not enough buffer memory.\n");
      return 1;
    }
    
    buffers = calloc(req.count, sizeof (*buffers));
    i = 0;
    //for (i = 0; i < req.count; i++) {
      struct v4l2_buffer buffer;
      
      memset(&buffer, 0, sizeof(buffer));
      buffer.type = req.type;
      buffer.memory = V4L2_MEMORY_MMAP;
      buffer.index = i;
      
      if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buffer)) {
        perror("VIDIOC_QUERYBUF");
        return 1;
      }
      
      buffers[i].length = buffer.length;
      printf("start:%p\n", buffers[i].start);
      buffers[i].start = mmap(NULL, buffer.length,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED,
                              fd, buffer.m.offset);
      printf("start:%p\n", buffers[i].start);      
      if (MAP_FAILED == buffers[i].start) {
        perror("mmap");
        return 1;
      }
    //}
    return 0;
}

static void imageProcess(const void* p);

int capture_image(int fd)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    printf("capture_image\n");
    if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    {
        perror("Query Buffer");
        return 1;
    }

    if(-1 == xioctl(fd, VIDIOC_STREAMON, &buf.type))
    {
        perror("Start Capture");
        return 1;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 2;
    int r = select(fd+1, &fds, NULL, NULL, &tv);
    printf("r:%d\n", r);
    if(-1 == r)
    {
        perror("Waiting for Frame");
        return 1;
    }

    if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
    {
        perror("Retrieving Frame");
        return 1;
    }

    /*
    int outfd = open("out.img", O_RDWR|O_EXCL);
    printf("start:%p\n", buffers[0].start);
    printf("outfd:%d, bytesused:%d\n", outfd, buf.bytesused);
    write(outfd, buffers[0].start, buf.bytesused);
    close(outfd);
    */
    
    imageProcess(buffers[0].start);
    
    return 0;
}

int check_input_output(int fd)
{
    struct v4l2_input input;
    int index;
    
    if (-1 == ioctl (fd, VIDIOC_G_INPUT, &index)) {
      perror ("VIDIOC_G_INPUT");
      return -1;
    }
    
    memset (&input, 0, sizeof(input));
    input.index = index;
    
    if (-1 == ioctl (fd, VIDIOC_ENUMINPUT, &input)) {
      perror ("VIDIOC_ENUMINPUT");
      return -1;
    }
    
    printf ("Current input: %s\n", input.name);
}

// global settings
static unsigned int width = 640;
static unsigned int height = 480;
static unsigned char jpegQuality = 70;
static char* jpegFilename = "camera.jpeg";
static char* deviceName = "/dev/video0";


/**
  Write image to jpeg file.

  \param img image to write
*/
static void jpegWrite(unsigned char* img)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
        
  JSAMPROW row_pointer[1];
  FILE *outfile = fopen( jpegFilename, "wb" );

  // try to open file for saving
  if (!outfile) {
    return;//errno_exit("jpeg");
  }

  // create jpeg data
  cinfo.err = jpeg_std_error( &jerr );
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, outfile);

  // set image parameters
  cinfo.image_width = width;    
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;

  // set jpeg compression parameters to default
  jpeg_set_defaults(&cinfo);
  // and then adjust quality setting
  jpeg_set_quality(&cinfo, jpegQuality, TRUE);

  // start compress 
  jpeg_start_compress(&cinfo, TRUE);

  // feed data
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &img[cinfo.next_scanline * cinfo.image_width *  cinfo.input_components];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  // finish compression
  jpeg_finish_compress(&cinfo);

  // destroy jpeg data
  jpeg_destroy_compress(&cinfo);

  // close output file
  fclose(outfile);
}

/**
  process image read
*/
static void imageProcess(const void* p)
{
  unsigned char* src = (unsigned char*)p;
  unsigned char* dst = malloc(width*height*3*sizeof(char));

  // convert from YUV422 to RGB888
  YUV422toRGB888(width,height,src,dst);

  // write jpeg
  jpegWrite(dst);
}


int main()
{
    int fd;
    int i;

    fd = open("/dev/video0", O_RDWR);
    printf("fd:%d\n", fd);
    if (fd == -1)
    {
            perror("Opening video device");
            return 1;
    }

    printf("print_cap()\n");
    if(print_caps(fd))
        return 1;
    
    check_input_output(fd);
    
    printf("print_mmap()\n");
    if(init_mmap(fd))
        return 1;

    check_input_output(fd);        
    
    printf("print_image()\n");
    if(capture_image(fd))
        return 1;

    for (i = 0; i < req.count; i++) {
      munmap(buffers[i].start, buffers[i].length);
    }
    close(fd);
    return 0;
}
