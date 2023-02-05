/*
 * Copyright (C) 2023 National Cheng Kung University, Taiwan.
 * Copyright (c) 2017 Daan Sprenkels <daan@dsprenkels.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* In the case that are compiling on linux, we need to define _GNU_SOURCE
 * *before* random.h is included. Otherwise SYS_getrandom will not be declared.
 */
#if defined(__linux__) || defined(__GNU__)
#define _GNU_SOURCE
#endif

#include "random.h"

#if defined(__linux__) || defined(__GNU__)
/* We would need to include <linux/random.h>, but not every target has access
 * to the linux headers. We only need RNDGETENTCNT, so we instead inline it.
 * RNDGETENTCNT is originally defined in `include/uapi/linux/random.h` in the
 * Linux kernel source.
 */
#define RNDGETENTCNT 0x80045200

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#if (defined(__linux__) || defined(__GNU__)) && defined(__GLIBC__) && \
    ((__GLIBC__ > 2) || (__GLIBC_MINOR__ > 24))
#define USE_GLIBC
#include <sys/random.h>
#endif
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

/* We need SSIZE_MAX as the maximum read len from /dev/urandom */
#if !defined(SSIZE_MAX)
#include <bits/wordsize.h>
#if __WORDSIZE == 64
#define SSIZE_MAX LONG_MAX
#else
#define SSIZE_MAX INT_MAX
#endif
#endif

#endif /* defined(__linux__) || defined(__GNU__) */

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
/* Dragonfly, FreeBSD, NetBSD, OpenBSD (has arc4random) */
#include <sys/param.h>
#if defined(BSD)
#include <stdlib.h>
#endif
/* GNU/Hurd defines BSD in sys/param.h which causes problems later */
#if defined(__GNU__)
#undef BSD
#endif
#endif

#if (defined(__linux__) || defined(__GNU__)) && \
    (defined(USE_GLIBC) || defined(SYS_getrandom))
#if defined(USE_GLIBC)
/* getrandom is declared in glibc. */
#elif defined(SYS_getrandom)
static ssize_t getrandom(void *buf, size_t buflen, unsigned int flags)
{
    return syscall(SYS_getrandom, buf, buflen, flags);
}
#endif

static int randombytes_linux_randombytes_getrandom(void *buf, size_t n)
{
    size_t offset = 0;
    int ret;
    while (n > 0) {
        /* getrandom does not allow chunks larger than 33554431 */
        size_t chunk = n <= 33554431 ? n : 33554431;
        do {
            ret = getrandom((char *) buf + offset, chunk, 0);
        } while (ret == -1 && errno == EINTR);
        if (ret < 0)
            return ret;
        offset += ret;
        n -= ret;
    }
    assert(n == 0);
    return 0;
}
#endif /* (defined(__linux__) || defined(__GNU__)) && (defined(USE_GLIBC) || \
          defined(SYS_getrandom)) */

#if defined(__linux__) && !defined(SYS_getrandom)

#if defined(__linux__)
static inline int randombytes_linux_read_entropy_ioctl(int device, int *entropy)
{
    return ioctl(device, RNDGETENTCNT, entropy);
}

static int randombytes_linux_read_entropy_proc(FILE *stream, int *entropy)
{
    int retcode;
    do {
        rewind(stream);
        retcode = fscanf(stream, "%d", entropy);
    } while (retcode != 1 && errno == EINTR);
    if (retcode != 1)
        return -1;
    return 0;
}

static int randombytes_linux_wait_for_entropy(int device)
{
    /* We will block on /dev/random, because any increase in the OS' entropy
     * level will unblock the request. I use poll here (as does libsodium),
     * because we don't *actually* want to read from the device.
     */
    enum { IOCTL, PROC } strategy = IOCTL;
    const int bits = 128;
    FILE *proc_file;
    int retcode_error = 0; /* Used as return codes */
    int entropy = 0;

    /* If the device has enough entropy already, we will want to return early */
    int retcode = randombytes_linux_read_entropy_ioctl(device, &entropy);
    if (retcode != 0 && (errno == ENOTTY || errno == ENOSYS)) {
        /* The ioctl call on /dev/urandom has failed due to a
         *   - ENOTTY (unsupported action), or
         *   - ENOSYS (invalid ioctl; this happens on MIPS).
         *
         * We will fall back to reading from
         * /proc/sys/kernel/random/entropy_avail . This is less ideal,
         * because it allocates a file descriptor, and it may not work
         * in a chroot.  But at this point it seems we have no better
         * options left.
         */
        strategy = PROC;
        /* Open the entropy count file */
        proc_file = fopen("/proc/sys/kernel/random/entropy_avail", "r");
        if (!proc_file)
            return -1;
    } else if (retcode != 0) {
        /* Unrecoverable ioctl error */
        return -1;
    }
    if (entropy >= bits)
        return 0;

    int fd;
    do {
        fd = open("/dev/random", O_RDONLY);
    } while (fd == -1 && errno == EINTR); /* EAGAIN will not occur */
    if (fd == -1) {
        /* Unrecoverable IO error */
        return -1;
    }

    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    for (;;) {
        retcode = poll(&pfd, 1, -1);
        if (retcode == -1 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        } else if (retcode == 1) {
            if (strategy == IOCTL) {
                retcode =
                    randombytes_linux_read_entropy_ioctl(device, &entropy);
            } else if (strategy == PROC) {
                retcode =
                    randombytes_linux_read_entropy_proc(proc_file, &entropy);
            } else {
                return -1; /* Unreachable */
            }

            if (retcode != 0) {
                /* Unrecoverable I/O error */
                retcode_error = retcode;
                break;
            }
            if (entropy >= bits)
                break;
        } else {
            /* Unreachable: poll() should only return -1 or 1 */
            retcode_error = -1;
            break;
        }
    }
    do {
        retcode = close(fd);
    } while (retcode == -1 && errno == EINTR);
    if (strategy == PROC) {
        do {
            retcode = fclose(proc_file);
        } while (retcode == -1 && errno == EINTR);
    }
    if (retcode_error != 0)
        return retcode_error;
    return retcode;
}
#endif /* defined(__linux__) */

static int randombytes_linux_randombytes_urandom(void *buf, size_t n)
{
    int fd;
    do {
        fd = open("/dev/urandom", O_RDONLY);
    } while (fd == -1 && errno == EINTR);
    if (fd == -1)
        return -1;
#if defined(__linux__)
    if (randombytes_linux_wait_for_entropy(fd) == -1)
        return -1;
#endif

    size_t offset = 0;
    while (n > 0) {
        size_t count = n <= SSIZE_MAX ? n : SSIZE_MAX;
        ssize_t tmp = read(fd, (char *) buf + offset, count);
        if (tmp == -1 && (errno == EAGAIN || errno == EINTR))
            continue;
        if (tmp == -1)
            return -1; /* Unrecoverable IO error */
        offset += tmp;
        n -= tmp;
    }
    close(fd);
    assert(n == 0);
    return 0;
}
#endif /* defined(__linux__) && !defined(SYS_getrandom) */

#if defined(BSD)
#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#if defined(MAC_OS_X_VERSION_10_10) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_10
#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonRandom.h>
#endif
#endif
static int randombytes_bsd_randombytes(void *buf, size_t n)
{
#if defined(MAC_OS_X_VERSION_10_15) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_15
    /* We prefere CCRandomGenerateBytes as it returns an error code while
     * arc4random_buf may fail silently on macOS.
     */
    return (CCRandomGenerateBytes(buf, n) == kCCSuccess);
#else
    arc4random_buf(buf, n);
    return 0;
#endif
}
#endif

int randombytes(uint8_t *buf, size_t n)
{
#if defined(__linux__) || defined(__GNU__)
#if defined(USE_GLIBC)
    /* Use getrandom system call */
    return randombytes_linux_randombytes_getrandom(buf, n);
#elif defined(SYS_getrandom)
    /* Use getrandom system call */
    return randombytes_linux_randombytes_getrandom(buf, n);
#else
    /* When we have enough entropy, we can read from /dev/urandom */
    return randombytes_linux_randombytes_urandom(buf, n);
#endif
#elif defined(BSD)
    return randombytes_bsd_randombytes(buf, n);
#else
#error "randombytes(...) is not supported on this platform"
#endif
}
