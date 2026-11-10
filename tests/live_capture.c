#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

enum { FRAME_COUNT = 5, BUFFER_COUNT = 2 };

static void require(int ok, const char *step) {
    if (ok) return;
    fprintf(stderr, "%s: %s\n", step, strerror(errno));
    exit(1);
}

int main(int argc, char **argv) {
    int fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
    require(fd >= 0, "open camera");
    fprintf(stderr, "camera opened\n");
    struct v4l2_capability cap = {0};
    require(ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0, "query capabilities");
    struct v4l2_format format = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    require(ioctl(fd, VIDIOC_G_FMT, &format) == 0, "query format");
    require(format.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24, "expected RGB24");
    require(ioctl(fd, VIDIOC_S_FMT, &format) == 0, "set format");
    struct v4l2_requestbuffers request = {
        .count = BUFFER_COUNT,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    require(ioctl(fd, VIDIOC_REQBUFS, &request) == 0 && request.count == BUFFER_COUNT,
            "allocate buffers");
    void *pixels[BUFFER_COUNT];
    size_t lengths[BUFFER_COUNT];
    for (unsigned i = 0; i < BUFFER_COUNT; i++) {
        struct v4l2_buffer buffer = {
            .index = i, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP,
        };
        require(ioctl(fd, VIDIOC_QUERYBUF, &buffer) == 0, "query buffer");
        lengths[i] = buffer.length;
        pixels[i] = mmap(NULL, buffer.length, PROT_READ, MAP_SHARED, fd, buffer.m.offset);
        require(pixels[i] != MAP_FAILED, "map buffer");
        require(ioctl(fd, VIDIOC_QBUF, &buffer) == 0, "queue buffer");
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    require(ioctl(fd, VIDIOC_STREAMON, &type) == 0, "start stream");
    fprintf(stderr, "stream started\n");
    for (unsigned frame = 0; frame < FRAME_COUNT; frame++) {
        struct pollfd ready = {.fd = fd, .events = POLLIN};
        require(poll(&ready, 1, 20000) > 0 && (ready.revents & POLLIN), "wait for frame");
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP,
        };
        require(ioctl(fd, VIDIOC_DQBUF, &buffer) == 0, "dequeue frame");
        require(buffer.index < BUFFER_COUNT && buffer.bytesused == format.fmt.pix.sizeimage,
                "frame layout");
        const unsigned char *image = pixels[buffer.index];
        uint64_t hash = UINT64_C(1469598103934665603);
        for (unsigned i = 0; i < buffer.bytesused; i++)
            hash = (hash ^ image[i]) * UINT64_C(1099511628211);
        printf("frame %u: %ux%u RGB24, %u bytes, hash %016llx\n", frame + 1,
                format.fmt.pix.width, format.fmt.pix.height, buffer.bytesused,
                (unsigned long long)hash);
        fflush(stdout);
        if (frame == 0 && argc > 1) {
            FILE *out = fopen(argv[1], "wb");
            require(out != NULL, "open frame output");
            fprintf(out, "P6\n%u %u\n255\n", format.fmt.pix.width, format.fmt.pix.height);
            require(fwrite(image, 1, buffer.bytesused, out) == buffer.bytesused, "write frame");
            require(fclose(out) == 0, "close frame output");
        }
        if (frame + 1 < FRAME_COUNT)
            require(ioctl(fd, VIDIOC_QBUF, &buffer) == 0, "requeue buffer");
    }
    require(ioctl(fd, VIDIOC_STREAMOFF, &type) == 0, "stop stream");
    for (unsigned i = 0; i < BUFFER_COUNT; i++)
        require(munmap(pixels[i], lengths[i]) == 0, "unmap buffer");
    require(close(fd) == 0, "close camera");
    return 0;
}
