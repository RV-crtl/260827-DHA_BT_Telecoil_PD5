/**
 * @file syscalls.c
 * @brief Minimal Newlib system-call stubs for the bare-metal STM32F446 target.
 *
 * The application and tests do not depend on a filesystem or process model. Explicit
 * stubs keep the link deterministic and prevent recent GNU Arm Embedded toolchains from
 * emitting unresolved/nosys diagnostics. Standard I/O is not used for the test console;
 * BoardConsole writes directly to USART2.
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char _end;
extern char _estack;

static char *g_heap_end;

/**
 * @brief Reject attempts to close a nonexistent bare-metal file descriptor.
 * @param file Ignored descriptor.
 * @return -1 with errno set to EBADF.
 */
int _close(int file)
{
    (void)file;
    errno = EBADF;
    return -1;
}

/**
 * @brief Report a valid descriptor as a character device for Newlib compatibility.
 * @param file Ignored descriptor.
 * @param st Destination status structure.
 * @return 0 on success, otherwise -1 for a NULL destination.
 */
int _fstat(int file, struct stat *st)
{
    (void)file;
    if (st == NULL) {
        errno = EINVAL;
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

/**
 * @brief Report descriptors as terminal-like character streams.
 * @param file Ignored descriptor.
 * @return 1.
 */
int _isatty(int file)
{
    (void)file;
    return 1;
}

/**
 * @brief Reject seek operations because no filesystem is present.
 * @param file Ignored descriptor.
 * @param offset Ignored offset.
 * @param whence Ignored seek origin.
 * @return -1 with errno set to ESPIPE.
 */
off_t _lseek(int file, off_t offset, int whence)
{
    (void)file;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return (off_t)-1;
}

/**
 * @brief Provide an empty non-blocking read stub; input is not used by this project.
 * @param file Ignored descriptor.
 * @param ptr Ignored destination buffer.
 * @param len Ignored requested length.
 * @return 0 bytes read.
 */
int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

/**
 * @brief Accept accidental Newlib writes without routing them through the application console.
 * @param file Ignored descriptor.
 * @param ptr Ignored source buffer.
 * @param len Requested byte count.
 * @return @p len for non-negative requests, otherwise zero.
 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    /* The project uses BoardConsole directly rather than stdio.  Returning the
     * requested length makes accidental Newlib writes benign and deterministic. */
    return (len < 0) ? 0 : len;
}

/**
 * @brief Provide a bounded heap-growth hook required by Newlib, although Application uses no heap.
 * @param increment Signed number of bytes requested from the linker-defined heap end.
 * @return Previous heap end on success, or `(void *)-1` with ENOMEM on overflow/stack collision.
 */
void *_sbrk(ptrdiff_t increment)
{
    if (g_heap_end == NULL) {
        g_heap_end = &_end;
    }

    char *const previous = g_heap_end;
    char *const next = g_heap_end + increment;
    /* Preserve the linker-reserved 4 KiB stack at the top of RAM. */
    if ((next < &_end) || (next > (&_estack - 0x1000))) {
        errno = ENOMEM;
        return (void *)-1;
    }
    g_heap_end = next;
    return previous;
}

/**
 * @brief Return a deterministic synthetic process identifier for Newlib compatibility.
 * @return Constant process ID 1.
 */
int _getpid(void)
{
    return 1;
}

/**
 * @brief Reject process signalling because no process model exists.
 * @param pid Ignored process identifier.
 * @param sig Ignored signal number.
 * @return -1 with errno set to EINVAL.
 */
int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}
