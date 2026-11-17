#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static void check(int ok, const char *step) {
    if (!ok) { fprintf(stderr, "%s: %s\n", step, strerror(errno)); exit(1); }
}

int main(void) {
    int fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
    check(fd >= 0, "open");
    struct v4l2_format format = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    check(ioctl(fd, VIDIOC_S_FMT, &format) == 0 &&
          format.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12 &&
          format.fmt.pix.sizeimage == 640 * 480 * 3 / 2, "NV12 format");
    struct v4l2_requestbuffers request = {
        .count = 2, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP,
    };
    check(ioctl(fd, VIDIOC_REQBUFS, &request) == 0 && request.count == 2, "buffers");
    void *pixels[2];
    for (unsigned i = 0; i < 2; ++i) {
        struct v4l2_buffer buffer = {
            .index = i, .type = request.type, .memory = request.memory,
        };
        check(ioctl(fd, VIDIOC_QUERYBUF, &buffer) == 0 &&
              buffer.length >= format.fmt.pix.sizeimage, "query buffer");
        pixels[i] = mmap(NULL, buffer.length, PROT_READ, MAP_SHARED, fd, buffer.m.offset);
        check(pixels[i] != MAP_FAILED, "mmap");
        check(ioctl(fd, VIDIOC_QBUF, &buffer) == 0, "queue buffer");
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    check(ioctl(fd, VIDIOC_STREAMON, &type) == 0, "stream on");
    fprintf(stderr, "NV12 640x480 stream started\n");
    for (;;) {
        struct pollfd ready = {.fd = fd, .events = POLLIN};
        check(poll(&ready, 1, 5000) > 0 && (ready.revents & POLLIN), "poll");
        struct v4l2_buffer buffer = {.type = type, .memory = V4L2_MEMORY_MMAP};
        check(ioctl(fd, VIDIOC_DQBUF, &buffer) == 0 &&
              buffer.index < 2 && buffer.bytesused == format.fmt.pix.sizeimage, "frame");
        const unsigned char *data = pixels[buffer.index];
        size_t left = buffer.bytesused;
        while (left) {
            ssize_t count = write(STDOUT_FILENO, data, left);
            if (count <= 0) return errno == EPIPE ? 0 : 1;
            data += count;
            left -= count;
        }
        check(ioctl(fd, VIDIOC_QBUF, &buffer) == 0, "requeue");
    }
}
