#include "str_io_posix.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>

long ds_posix_write_cb(void *ud, const void *buf, size_t n) {
  int fd = (int)(long)ud;
  const char *p = (const char*)buf;
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(fd, p + off, n - off);
    if (w < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (w == 0) return -1;
    off += (size_t)w;
  }
  return (long)off;
}

long ds_posix_writev_cb(void *ud,
                        const void * const *bufs,
                        const size_t *lens,
                        size_t count) {
  struct iovec vec[16];
  size_t off = 0;
  int fd = (int)(long)ud;

  while (off < count) {
    size_t batch = count - off;
    size_t i;
    if (batch > 16) batch = 16;

    for (i = 0; i < batch; ++i) {
      const void *srcp = bufs[off + i];
      void *dstp;
      memcpy(&dstp, &srcp, sizeof(void*));
      vec[i].iov_base = dstp;
      vec[i].iov_len  = lens[off + i];
    }

    for (;;) {
      ssize_t w = writev(fd, vec, (int)batch);
      if (w < 0) {
        if (errno == EINTR) continue;
        return -1;
      }
      if (w == 0) return -1;

      {
        ssize_t remain = w;
        size_t k = 0;
        while (k < batch && remain > 0) {
          if (remain >= (ssize_t)vec[k].iov_len) {
            remain -= (ssize_t)vec[k].iov_len;
            ++k;
          } else {
            vec[k].iov_base = (char*)vec[k].iov_base + remain;
            vec[k].iov_len  -= (size_t)remain;
            remain = 0;
          }
        }
        if (k == batch) break;
        if (k > 0) {
          size_t m;
          for (m = 0; k + m < batch; ++m) vec[m] = vec[k + m];
          batch -= k;
        }
      }
    }

    off += batch;
  }
  return 0;
}
