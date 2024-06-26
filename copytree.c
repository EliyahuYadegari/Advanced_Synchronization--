// copytree.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "copytree.h"

void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    // Open source file for reading
    int fd_src = open(src, O_RDONLY);
    if (fd_src == -1) {
        perror("open");
        return;
    }

    // Get file status to check if it's a symlink
    struct stat src_stat;
    if (lstat(src, &src_stat) == -1) {
        perror("lstat");
        close(fd_src);
        return;
    }

    // Handle symbolic link if not copying symlinks
    if (!copy_symlinks && S_ISLNK(src_stat.st_mode)) {
        fprintf(stderr, "Not copying symbolic link: %s\n", src);
        close(fd_src);
        return;
    }

    // Open destination file for writing, create if it doesn't exist
    int fd_dest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, src_stat.st_mode & 0777);
    if (fd_dest == -1) {
        perror("open");
        close(fd_src);
        return;
    }

    // Copy contents from source to destination
    char buf[4096];
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(fd_src, buf, sizeof(buf))) > 0) {
        bytes_written = write(fd_dest, buf, bytes_read);
        if (bytes_written != bytes_read) {
            perror("write");
            close(fd_src);
            close(fd_dest);
            return;
        }
    }

    // Check for read error
    if (bytes_read == -1) {
        perror("read");
        close(fd_src);
        close(fd_dest);
        return;
    }

    // Close file descriptors
    close(fd_src);
    close(fd_dest);

    // Set permissions if requested
    if (copy_permissions && chmod(dest, src_stat.st_mode & 0777) == -1) {
        perror("chmod");
        return;
    }
}

int mkdir_recursive(const char *path, mode_t mode) {
    // Create directories recursively
    char *ptr = NULL;
    char *temp = strdup(path);
    int ret = 0;

    // Iterate through each component of the path and create directories
    for (ptr = temp + 1; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = '\0';  // temporarily truncate the path
            if (mkdir(temp, mode) == -1 && errno != EEXIST) {
                perror("mkdir");
                ret = -1;
                break;
            }
            *ptr = '/';
        }
    }

    // Create the final directory
    if (mkdir(temp, mode) == -1 && errno != EEXIST) {
        perror("mkdir");
        ret = -1;
    }

    free(temp);
    return ret;
}

void copy_directory(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    // Create destination directory if it doesn't exist
    if (mkdir_recursive(dest, 0777) == -1) {
        perror("mkdir_recursive");
        return;
    }

    // Open source directory
    DIR *dir = opendir(src);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    // Iterate through directory entries
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full paths for source and destination
        char src_path[PATH_MAX];
        char dest_path[PATH_MAX];
        snprintf(src_path, PATH_MAX, "%s/%s", src, entry->d_name);
        snprintf(dest_path, PATH_MAX, "%s/%s", dest, entry->d_name);

        // Get file status
        struct stat statbuf;
        if (lstat(src_path, &statbuf) == -1) {
            perror("lstat");
            closedir(dir);
            return;
        }

        // Copy file or recursively copy directory
        if (S_ISDIR(statbuf.st_mode)) {
            // Recursively copy directory
            copy_directory(src_path, dest_path, copy_symlinks, copy_permissions);
        } else if (S_ISREG(statbuf.st_mode)) {
            // Copy regular file
            copy_file(src_path, dest_path, copy_symlinks, copy_permissions);
        } else if (S_ISLNK(statbuf.st_mode)) {
            // Handle symbolic link
            if (copy_symlinks) {
                char link_target[PATH_MAX];
                ssize_t len = readlink(src_path, link_target, sizeof(link_target) - 1);
                if (len == -1) {
                    perror("readlink");
                    closedir(dir);
                    return;
                }
                link_target[len] = '\0';

                if (symlink(link_target, dest_path) == -1) {
                    perror("symlink");
                    closedir(dir);
                    return;
                }
            } else {
                fprintf(stderr, "Not copying symbolic link: %s\n", src_path);
            }
        }
    }

    // Check for errors in readdir
    if (errno != 0) {
        perror("readdir");
    }

    // Close directory
    closedir(dir);
}
