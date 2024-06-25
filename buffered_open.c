#include "buffered_open.h"
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>


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
        va_start(args, flags);
        preappend = va_arg(args, int);
        va_end(args);
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
    bf->read_buffer_size = BUFFER_SIZE;
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

    // If O_PREAPPEND is set, move the file offset to the end of the file
    if (preappend) {
        off_t offset = lseek(fd, 0, SEEK_END);
        if (offset == -1) {
            free(bf->read_buffer);
            free(bf->write_buffer);
            free(bf);
            close(fd);
            return NULL; // Error handling for lseek
        }
    }

    return bf;
}

// Function to write to the buffered file
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count) {
    size_t remaining = count;
    const char *buf_ptr = (const char *)buf;

    // Flush any pending writes before proceeding with the write operation
    if (buffered_flush(bf) == -1) {
        return -1; // Error during flush
    }

    // Handle O_PREAPPEND flag
   // printf ("preappend: %d", O_PREAPPEND);
    if (bf->flags & O_PREAPPEND) {
        // printf ("preappend2:");
        // Step 1: Read existing file content into a temporary buffer
        off_t file_size = lseek(bf->fd, 0, SEEK_END);
        if (file_size == -1) {
            return -1; // Error handling for lseek
        }

        char *temp_buffer = (char *)malloc(file_size);
        if (!temp_buffer) {
            errno = ENOMEM;
            return -1; // Error handling for malloc
        }

        ssize_t bytes_read = pread(bf->fd, temp_buffer, file_size, 0);
        if (bytes_read == -1) {
            free(temp_buffer);
            return -1; // Error handling for pread
        }
        //printf ("temp buffer: %s" , temp_buffer);

        // Step 2: Write new data at the beginning
        ssize_t write_result = pwrite(bf->fd, buf_ptr, count, 0);
        if (write_result == -1) {
            free(temp_buffer);
            return -1; // Error handling for pwrite
        }

        // Step 3: Append the existing content after the new data
        write_result = pwrite(bf->fd, temp_buffer, bytes_read, count);
        free(temp_buffer);
        if (write_result == -1) {
            return -1; // Error handling for pwrite
        }

        // Update buffered_file_t state
        bf->write_buffer_pos = 0;

        return count;
    }

    // Normal buffered write logic without O_PREAPPEND
    while (remaining > 0) {
        size_t space_in_buffer = bf->write_buffer_size - bf->write_buffer_pos;

        // If the buffer is full, flush it
        if (space_in_buffer == 0) {
            if (buffered_flush(bf) == -1) {
                return -1; // Error occurred during flush
            }
            space_in_buffer = bf->write_buffer_size;
        }

        // Determine how much data can be written to the buffer
        size_t to_write = (remaining < space_in_buffer) ? remaining : space_in_buffer;

        // Copy data to the buffer
        memcpy(bf->write_buffer + bf->write_buffer_pos, buf_ptr, to_write);
        bf->write_buffer_pos += to_write;
        buf_ptr += to_write;
        remaining -= to_write;
    }

    return count;
}

// Function to flush the buffer to the file
int buffered_flush(buffered_file_t *bf) {
    if (bf->write_buffer_pos > 0) {
        // Flush the current buffer content
        ssize_t written = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
        if (written == -1) {
            return -1; // Error occurred during write
        }

        // Handle O_PREAPPEND flag
        if (bf->preappend) {
            // Read existing file content into a temporary buffer
            off_t current_offset = lseek(bf->fd, 0, SEEK_CUR);
            if (current_offset == -1) {
                return -1; // Error handling for lseek
            }

            char *temp_buffer = (char *)malloc(current_offset);
            if (!temp_buffer) {
                errno = ENOMEM;
                return -1; // Error handling for malloc
            }

            ssize_t bytes_read = pread(bf->fd, temp_buffer, current_offset, 0);
            if (bytes_read == -1) {
                free(temp_buffer);
                return -1; // Error handling for pread
            }

            // Write existing content after the flushed buffer content
            ssize_t write_result = pwrite(bf->fd, temp_buffer, bytes_read, bf->write_buffer_pos);
            free(temp_buffer);
            if (write_result == -1) {
                return -1; // Error handling for pwrite
            }

            bf->write_buffer_pos = 0;
        } else {
            // Normal buffered flush without O_PREAPPEND
            bf->write_buffer_pos = 0;
        }
    }
    return 0;
}

// Function to read from the buffered file
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count) {
    size_t remaining = count;
    char *buf_ptr = (char *)buf;

    // Flush write buffer before reading
    if (buffered_flush(bf) == -1) {
        return -1; // Error occurred during flush
    }

    while (remaining > 0) {
        size_t available_in_buffer = bf->read_buffer_size - bf->read_buffer_pos;

        // If the buffer is empty, fill it
        if (available_in_buffer == 0) {
            ssize_t bytes_read = read(bf->fd, bf->read_buffer, BUFFER_SIZE);
            if (bytes_read == -1) {
                return -1; // Error occurred during read
            }
            if (bytes_read == 0) {
                break; // End of file reached
            }
            bf->read_buffer_pos = 0;
            bf->read_buffer_size = bytes_read;
            available_in_buffer = bytes_read;
        }

        // Determine how much data can be read from the buffer
        size_t to_read = (remaining < available_in_buffer) ? remaining : available_in_buffer;

        // Copy data from the buffer to the output buffer
        memcpy(buf_ptr, bf->read_buffer + bf->read_buffer_pos, to_read);
        bf->read_buffer_pos += to_read;
        buf_ptr += to_read;
        remaining -= to_read;
    }

    // Update file offset after reading
    off_t new_offset = lseek(bf->fd, 0, SEEK_CUR); // Get current offset
    if (new_offset == -1) {
        return -1; // Error handling for lseek
    }

    return count - remaining;
}

// Function to close the buffered file
int buffered_close(buffered_file_t *bf) {
    // Flush any remaining data in the write buffer to the file
    int flush_result = buffered_flush(bf);
    if (flush_result == -1) {
        return -1; // Error occurred during flush
    }

    // Close the file descriptor
    int close_result = close(bf->fd);

    // Free allocated memory
    free(bf->read_buffer);
    free(bf->write_buffer);
    free(bf);

    return close_result;
}