#include <errno.h>
#include <jni.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

JNIEXPORT jint JNICALL
Java_org_randomcoder_udroid_camera_CameraBrokerBridge_connect(JNIEnv *env, jclass type, jstring path) {
    (void)type;
    if (!path) return -EINVAL;
    const char *name = (*env)->GetStringUTFChars(env, path, NULL);
    if (!name) return -ENOMEM;
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    int fd = -ENAMETOOLONG;
    size_t length = strlen(name);
    if (length < sizeof(address.sun_path)) {
        memcpy(address.sun_path, name, length + 1);
        fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
        if (fd >= 0 && connect(fd, (const struct sockaddr *)&address,
                offsetof(struct sockaddr_un, sun_path) + length + 1) < 0) {
            int error = errno;
            close(fd);
            fd = -error;
        }
    }
    (*env)->ReleaseStringUTFChars(env, path, name);
    return fd;
}
