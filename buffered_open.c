#include "buffered_open.h"
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

// Function to wrap the original open function
buffered_file_t *buffered_open(const char *pathname, int flags, ...) {
    va_list args; // declarition of variable (args) va_list type
    va_start(args, flags); // add to "args" all the args after "flags"

    int mode = 0;
    // check if their is need to creat the file if not exist
    if (flags & O_CREAT) {
        mode = va_arg(args, int);
    }
    // create a buffered_file_t var
    buffered_file_t *bf = malloc(sizeof(buffered_file_t));
    if (!bf) {
        errno = ENOMEM;
        return NULL;
    }
    // preappend flag
    bf->preappend = (flags & O_PREAPPEND) ? 1 : 0;
    flags &= ~O_PREAPPEND; // Remove O_PREAPPEND flag before calling open
    // open the file and save the fd
    bf->fd = open(pathname, flags, mode);
    if (bf->fd == -1) {
        free(bf);
        return NULL;
    }
    // allocate memory for read_buffer, write_buffer
    bf->read_buffer = malloc(BUFFER_SIZE);
    bf->write_buffer = malloc(BUFFER_SIZE);
    if (!bf->read_buffer || !bf->write_buffer) {
        close(bf->fd);
        free(bf->read_buffer);
        free(bf->write_buffer);
        free(bf);
        errno = ENOMEM;
        return NULL;
    }
    bf->read_buffer_size = BUFFER_SIZE;     // assign read_buffer_size to BUFFER_SIZE
    bf->write_buffer_size = BUFFER_SIZE;    // assign write_buffer_size to BUFFER_SIZE
    bf->read_buffer_pos = 0;    // initial the pos to 0
    bf->write_buffer_pos = 0;   // initial the pos to 0
    bf->flags = flags;  // save the flags

    va_end(args);
    return bf;
}
// Function to write to the buffered file
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count) {
    if (!bf || !buf) {
        errno = EINVAL;
        return -1;
    }

    size_t bytes_written = 0;   // how many bytes where written
    const char *buffer = (const char *)buf;

    size_t space_left = bf->write_buffer_size - bf->write_buffer_pos;   // how much space left in the write buffer
    if(count > space_left){
        // flash the data frome the buffer to the file and then write to the empty buffer the new data
        if (buffered_flush(bf) == -1) {
            return -1;
        }
    }
    size_t to_write = count;
    // copy to_write from the input buffer to the write_buffer
    memcpy(bf->write_buffer + bf->write_buffer_pos, buffer + bytes_written, to_write);
    bf->write_buffer_pos += to_write;   // update the write_buffer_pos
    bytes_written += to_write;

    return bytes_written;
}

ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count) {
    if (!bf || !buf) {
        errno = EINVAL;
        return -1;
    }
    
    // if (bf->write_buffer_pos > 0) {
    //     if (buffered_flush(bf) == -1) {
    //         return -1;
    //     }
    // }
    
    size_t bytes_read = 0;
    char *buffer = (char *)buf;

    while (count > 0) {
        if (bf->read_buffer_pos == 0) {
            ssize_t n = read(bf->fd, bf->read_buffer, bf->read_buffer_size);
            if (n == -1) {
                return -1;
            } else if (n == 0) {
                break;
            }
            bf->read_buffer_pos = n;
        }
        size_t to_read = (count < bf->read_buffer_pos) ? count : bf->read_buffer_pos;
        memcpy(buffer + bytes_read, bf->read_buffer + (bf->read_buffer_size - bf->read_buffer_pos), to_read);
        bf->read_buffer_pos -= to_read;
        bytes_read += to_read;
        count -= to_read;
    }
    return bytes_read;
}

int buffered_flush(buffered_file_t *bf) {
    if (!bf) {
        errno = EINVAL;
        return -1;
    }

    if (bf->preappend && bf->write_buffer_pos > 0) {
        off_t original_offset = lseek(bf->fd, 0, SEEK_END);
        if (original_offset == (off_t)-1) {
            return -1;
        }

        char *temp_buffer = malloc(original_offset);
        if (!temp_buffer) {
            errno = ENOMEM;
            return -1;
        }

        if (lseek(bf->fd, 0, SEEK_SET) == (off_t)-1) {
            free(temp_buffer);
            return -1;
        }

        ssize_t n = read(bf->fd, temp_buffer, original_offset);
        if (n == -1) {
            free(temp_buffer);
            return -1;
        }

        if (lseek(bf->fd, 0, SEEK_SET) == (off_t)-1) {
            free(temp_buffer);
            return -1;
        }

        ssize_t w = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
        if (w == -1) {
            free(temp_buffer);
            return -1;
        }

        w = write(bf->fd, temp_buffer, n);
        if (w == -1) {
            free(temp_buffer);
            return -1;
        }

        free(temp_buffer);
        bf->write_buffer_pos = 0;
    } else if (bf->write_buffer_pos > 0) {
        ssize_t n = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
        if (n == -1) {
            return -1;
        }
        bf->write_buffer_pos = 0;
    }

    return 0;
}

int buffered_close(buffered_file_t *bf) {
    if (!bf) {
        errno = EINVAL;
        return -1;
    }

    if (buffered_flush(bf) == -1) {
        return -1;
    }

    if (close(bf->fd) == -1) {
        return -1;
    }

    free(bf->read_buffer);
    free(bf->write_buffer);
    free(bf);

    return 0;
}
