#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#define BUF_SIZE 4096
// io61.c
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd;
    char buff[BUF_SIZE];
    off_t tag;      // Offset in file of first byte in cache
    off_t end_tag;  // Offset in file of first INVALID byte in cache
    off_t pos_tag;  // Offset in file of next byte to read in cache.
};


// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = (io61_file*) malloc(sizeof(io61_file));
    f->fd = fd;
    (void) mode;
    return f;
}


// io61_close(f)
//    Close the io61_file `f` and release all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    free(f);
    return r;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
    unsigned char buf[1];
    if (io61_read(f, buf, 1) == 1)
        return buf[0];
    else
        return EOF;
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read. Heavily "inspired" by Q20 of Exercises Storage 3X.

ssize_t io61_read(io61_file* f, char* buf, size_t sz)
{
    ssize_t bytes_read = 0; // Update as we read data and is the ret value
    while (bytes_read != sz) {
        if (f->pos_tag < f->end_tag) { //Buffer still has valid data
            ssize_t to_read = sz - bytes_read;
            if (to_read > f->end_tag - f->pos_tag) //If buff doesn't have all
                to_read = f->end_tag - f->pos_tag; //Only read til end of buf
            memcpy(&buf[bytes_read], &f->buff[f->pos_tag - f->tag], to_read);
            f->pos_tag += to_read;
            bytes_read += to_read;
        } else { // Buffer needs to be refilled
            f->tag = f->end_tag;
            ssize_t read_res = read(f->fd, f->buff, BUF_SIZE);
            if (read_res > 0)
                f->end_tag += read_res;
            else // EOF or read() failed
                // Return bytes read or result of read() if none has been read
                return bytes_read ? bytes_read : read_res;
        }
    }
    return bytes_read;
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    char buf[1];
    buf[0] = ch;
    if (io61_write(f, buf, 1) == 1)
        return 0;
    else
        return -1;
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

// ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
//     size_t nwritten = 0;
//     while (nwritten != sz) {
//         if (io61_writec(f, buf[nwritten]) == -1)
//             break;
//         ++nwritten;
//     }
//     if (nwritten != 0 || sz == 0)
//         return nwritten;
//     else
//         return -1;
// }

//ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
//    ssize_t res = write(f->fd, buf, sz);
//    return res;
//}

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
   size_t bytes_read = 0; // Update as we read data and is the ret value
   while (bytes_read != sz) { // if we haven't already read sz amount of data
       if (f->pos_tag - f->tag < BUF_SIZE) {
           ssize_t n = sz - bytes_read; //set n as the number of bytes left to read
           if (BUF_SIZE - (f->pos_tag - f->tag) < n) //if n is greater than size of buffer left to write
               n = BUF_SIZE - (f->pos_tag - f->tag); //set n as the size of buffer left to write
           memcpy(&f->buff[f->pos_tag - f->tag], &buf[bytes_read], n);
           f->pos_tag += n;
           if (f->pos_tag > f->end_tag)
                     f->end_tag = f->pos_tag;
           bytes_read += n;
       }
       assert(f->pos_tag <= f->end_tag);
       if (f->pos_tag - f->tag == BUF_SIZE) //if we wrote everything in the buffer
           io61_flush(f); //flush f
   }

   return bytes_read;
}

// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    if (f->end_tag != f->tag) {
        ssize_t n = write(f->fd, f->buff, f->end_tag - f->tag);
        assert(n == f->end_tag - f->tag);
    }
    f->pos_tag = f->tag = f->end_tag;
    return 0;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t off) {
    // if the offset is not contained in the parameters,
    // change the offset by the amount it is off by
    // if this change fails, return -1
    if (off < f->tag || off > f->end_tag) {
        off_t aligned_off = off - (off % BUF_SIZE);
        off_t r = lseek(f->fd, aligned_off, SEEK_SET);
        if (r != aligned_off)
            return -1;
        f->tag = f->end_tag = aligned_off;
    }
    // set the position as the offset
    f->pos_tag = off;
    return 0;
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `filename == NULL`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != NULL` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename)
        fd = open(filename, mode, 0666);
    else if ((mode & O_ACCMODE) == O_RDONLY)
        fd = STDIN_FILENO;
    else
        fd = STDOUT_FILENO;
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode))
        return s.st_size;
    else
        return -1;
}


// io61_eof(f)
//    Test if readable file `f` is at end-of-file. Should only be called
//    immediately after a `read` call that returned 0 or -1.

int io61_eof(io61_file* f) {
    char x;
    ssize_t nread = read(f->fd, &x, 1);
    if (nread == 1) {
        fprintf(stderr, "Error: io61_eof called improperly\n\
  (Only call immediately after a read() that returned 0 or -1.)\n");
        abort();
    }
    return nread == 0;
}
