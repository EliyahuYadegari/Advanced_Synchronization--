#include "buffered_open.h"
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

// Function to wrap the original open function
buffered_file_t *buffered_open(const char *pathname, int flags, ...) {
    int mode = 0;
    va_list args;

    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }

    int preappend = 0;
    if (flags & O_PREAPPEND) {
        preappend = 1;
        flags |= O_RDWR;
        flags &= ~O_PREAPPEND;
    }

    // Call the original open function
    int fd = open(pathname, flags, mode);
    if (fd == -1) {
        return NULL;
    }

    // Allocate memory for buffered_file_t structure
    buffered_file_t *bf = (buffered_file_t *)malloc(sizeof(buffered_file_t));
    if (!bf) {
        close(fd);
        errno = ENOMEM;
        return NULL;
    }

    // Initialize the buffered_file_t structure
    bf->fd = fd;
    bf->flags = flags;
    bf->preappend = preappend;
    bf->read_buffer_size = 0;
    bf->write_buffer_size = BUFFER_SIZE;
    bf->read_buffer_pos = 0;
    bf->write_buffer_pos = 0;

    // Allocate memory for read and write buffers
    bf->read_buffer = (char *)malloc(BUFFER_SIZE);
    bf->write_buffer = (char *)malloc(BUFFER_SIZE);

    if (!bf->read_buffer || !bf->write_buffer) {
        free(bf->read_buffer);
        free(bf->write_buffer);
        free(bf);
        close(fd);
        errno = ENOMEM;
        return NULL;
    }

    return bf;
}

// Function to write to the buffered file
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count) {
    if (!bf || bf->fd < 0) {
        errno = EBADF;
        perror("buffered_write");
        return -1;
    }

    // Prepend logic
    if (bf->preappend) {
        // Check the file descriptor
        if (fcntl(bf->fd, F_GETFD) == -1) {
            perror("buffered_write fcntl");
            return -1;
        }

        // Read existing content
        off_t current_offset = lseek(bf->fd, 0, SEEK_END);
        if (current_offset == -1) {
            perror("lseek SEEK_END");
            return -1;
        }

        char *temp_buffer = malloc(current_offset);
        if (!temp_buffer) {
            perror("malloc");
            return -1;
        }

        if (lseek(bf->fd, 0, SEEK_SET) == -1) {
            perror("lseek SEEK_SET");
            free(temp_buffer);
            return -1;
        }

        ssize_t read_bytes = read(bf->fd, temp_buffer, current_offset);
        if (read_bytes == -1) {
            perror("read");
            free(temp_buffer);
            return -1;
        }

        // Clear the file content
        if (ftruncate(bf->fd, 0) == -1) {
            perror("ftruncate");
            free(temp_buffer);
            return -1;
        }

        if (lseek(bf->fd, 0, SEEK_SET) == -1) {
            perror("lseek SEEK_SET");
            free(temp_buffer);
            return -1;
        }

        // Write new content directly if larger than buffer
        if (count >= bf->write_buffer_size) {
            ssize_t written_bytes = write(bf->fd, buf, count);
            if (written_bytes == -1) {
                perror("write");
                free(temp_buffer);
                return -1;
            }

            ssize_t appended_bytes = write(bf->fd, temp_buffer, read_bytes);
            free(temp_buffer);

            if (appended_bytes == -1) {
                perror("write");
                return -1;
            }

            return written_bytes;
        }

        // Buffer the new content
        memcpy(bf->write_buffer, buf, count);
        bf->write_buffer_pos = count;

        // Flush buffer to file
        if (buffered_flush(bf) == -1) {
            free(temp_buffer);
            return -1;
        }

        // Append the existing content
        ssize_t appended_bytes = write(bf->fd, temp_buffer, read_bytes);
        free(temp_buffer);

        if (appended_bytes == -1) {
            perror("write");
            return -1;
        }

        return count;
    } else {
        // Regular buffered write logic
        size_t remaining_space = bf->write_buffer_size - bf->write_buffer_pos;

        // If count is larger than buffer size, write directly
        if (count >= bf->write_buffer_size) {
            // Flush any existing buffer content
            if (buffered_flush(bf) == -1) {
                return -1;
            }
            return write(bf->fd, buf, count);
        }

        // Flush if not enough space
        if (count > remaining_space) {
            if (buffered_flush(bf) == -1) {
                return -1;
            }
        }

        // Buffer the data
        memcpy(bf->write_buffer + bf->write_buffer_pos, buf, count);
        bf->write_buffer_pos += count;
        return count;
    }
}

// Function to flush the buffer to the file
int buffered_flush(buffered_file_t *bf) {
    if (!bf || bf->fd < 0) {
        errno = EBADF;
        perror("buffered_flush");
        return -1;
    }

    if (bf->preappend && bf->write_buffer_pos > 0) {
        // Check the file descriptor
        if (fcntl(bf->fd, F_GETFD) == -1) {
            perror("buffered_flush fcntl");
            return -1;
        }

        // Read existing content
        off_t current_offset = lseek(bf->fd, 0, SEEK_END);
        if (current_offset == -1) {
            perror("lseek SEEK_END");
            return -1;
        }

        char *temp_buffer = malloc(current_offset);
        if (!temp_buffer) {
            perror("malloc");
            return -1;
        }

        if (lseek(bf->fd, 0, SEEK_SET) == -1) {
            perror("lseek SEEK_SET");
            free(temp_buffer);
            return -1;
        }

        ssize_t read_bytes = read(bf->fd, temp_buffer, current_offset);
        if (read_bytes == -1) {
            perror("read");
            free(temp_buffer);
            return -1;
        }

        // Write new content
        if (lseek(bf->fd, 0, SEEK_SET) == -1) {
            perror("lseek SEEK_SET");
            free(temp_buffer);
            return -1;
        }

        ssize_t written_bytes = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
        if (written_bytes == -1) {
            perror("write");
            free(temp_buffer);
            return -1;
        }

        // Append existing content
        ssize_t appended_bytes = write(bf->fd, temp_buffer, read_bytes);
        free(temp_buffer);

        if (appended_bytes == -1) {
            perror("write");
            return -1;
        }

        bf->write_buffer_pos = 0;
        return 0;
    } else if (bf->write_buffer_pos > 0) {
        ssize_t written = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
        if (written == -1) {
            perror("buffered_flush: write error");
            return -1;
        }
        bf->write_buffer_pos = 0;
    }
    return 0;
}

// Function to read from the buffered file
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count) {
    if (!bf || bf->fd < 0) {
        errno = EBADF;
        perror("buffered_read");
        return -1;
    }

    size_t total_read = 0;
    char *user_buf = buf;

    while (count > 0) {
        size_t available_data = bf->read_buffer_size - bf->read_buffer_pos;

        if (available_data == 0) {
            ssize_t bytes_read = read(bf->fd, bf->read_buffer, BUFFER_SIZE);
            if (bytes_read == -1) {
                perror("buffered_read: read error");
                return -1;
            }
            if (bytes_read == 0) {
                break; // End of file
            }
            bf->read_buffer_pos = 0;
            bf->read_buffer_size = bytes_read;
            available_data = bytes_read;
        }

        size_t to_copy = (count < available_data) ? count : available_data;
        memcpy(user_buf, bf->read_buffer + bf->read_buffer_pos, to_copy);

        bf->read_buffer_pos += to_copy;
        user_buf += to_copy;
        count -= to_copy;
        total_read += to_copy;
    }

    return total_read;
}

// Function to close the buffered file
int buffered_close(buffered_file_t *bf) {
    if (buffered_flush(bf) == -1) {
        return -1;
    }

    int result = close(bf->fd);
    if (result == -1) {
        perror("close");
        return -1;
    }

    free(bf->read_buffer);
    free(bf->write_buffer);
    free(bf);
    return 0;
}
