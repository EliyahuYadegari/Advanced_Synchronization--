#include "copytree.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

// Function to delete a file or directory recursively
int delete_path(const char *path) {
    struct stat statbuf;
    if (lstat(path, &statbuf) != 0) {
        perror("stat");
        return -1;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        // It's a directory, so remove its contents first
        DIR *dir = opendir(path);
        if (!dir) {
            perror("opendir");
            return -1;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip the special entries "." and ".."
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            // Construct the full path of the entry
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

            // Recursively delete the entry
            if (delete_path(full_path) != 0) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);

        // Now the directory is empty, so remove it
        if (rmdir(path) != 0) {
            perror("rmdir");
            return -1;
        }
    } else {
        // It's a file or symbolic link, so remove it
        if (unlink(path) != 0) {
            perror("unlink");
            return -1;
        }
    }

    return 0;
}

// Function to check if the directory is empty
int is_directory_empty(const char *path) {
    int n = 0;
    struct dirent *d;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        return 1; // If directory can't be opened, assume it is empty or doesn't exist
    }

    while ((d = readdir(dir)) != NULL) {
        if (++n > 2) {
            break; // More than 2 entries means it is not empty (entries are "." and "..")
        }
    }
    closedir(dir);
    return n <= 2;
}

// Function to check if directory exists
int directory_exists(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

// Function to copy a file from src to dest, with options to handle symlinks and permissions.
void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    struct stat src_stat;
    if (lstat(src, &src_stat) == -1) {
        perror("Error getting source file information");
        return;
    }

    // Check if src is a directory
    if (S_ISDIR(src_stat.st_mode)) {
        fprintf(stderr, "Error: %s is a directory\n", src);
        return;
    }

    // Check if src is a symbolic link
    if (S_ISLNK(src_stat.st_mode)) {
        // Handle symbolic links
        if (copy_symlinks) {
            // Get the target of the symbolic link
            char link_target[PATH_MAX + 1];
            ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
            if (len == -1) {
                perror("Error reading symbolic link");
                return;
            }
            link_target[len] = '\0';

            // Create the symbolic link in the destination
            if (symlink(link_target, dest) == -1) {
                perror("Error creating symbolic link");
                return;
            }
        } else {
            // Error message indicating symbolic links are not supported
            fprintf(stderr, "Error: Symbolic link encountered but not copying as link (-l not specified)\n");
            return;
        }
    } else if (S_ISREG(src_stat.st_mode)) {
        // Regular file, copy its contents
        int source_fd = open(src, O_RDONLY);
        if (source_fd == -1) {
            perror("Error opening source file");
            fprintf(stderr, "File: %s\n", src); // Print the filename
            return;
        }

        // Open the target file for writing (create if it doesn't exist)
        int target_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, src_stat.st_mode);
        if (target_fd == -1) {
            perror("Error opening target file");
            fprintf(stderr, "File: %s\n", dest); // Print the filename
            close(source_fd);
            return;
        }

        // Copy the contents of the file
        char buffer[4096];
        ssize_t bytes_read, bytes_written;
        while ((bytes_read = read(source_fd, buffer, sizeof(buffer))) > 0) {
            bytes_written = write(target_fd, buffer, bytes_read);
            if (bytes_written != bytes_read) {
                perror("Error writing to target file");
                fprintf(stderr, "File: %s\n", dest); // Print the filename
                close(source_fd);
                close(target_fd);
                return;
            }
        }

        if (bytes_read == -1) {
            perror("Error reading from source file");
            close(source_fd);
            close(target_fd);
            return;
        }

        // Close the files
        close(source_fd);
        close(target_fd);
    } else {
        // Handle other file types if necessary (sockets, devices, etc.)
        fprintf(stderr, "Unsupported file type: %s\n", src);
    }

    // Set permissions of the target file if copy_permissions is enabled
    if (copy_permissions && !S_ISLNK(src_stat.st_mode)) {
        if (chmod(dest, src_stat.st_mode) == -1) {
            perror("Error setting target file permissions");
            fprintf(stderr, "File: %s\n", dest); // Print the filename
            return;
        }
    }
}

// Helper function to create directories recursively
int create_directory_recursive(const char *dir_path, mode_t mode) {
    char temp_path[PATH_MAX];
    char *ptr = NULL;
    size_t len;

    // Copy the directory path to a temporary buffer
    snprintf(temp_path, sizeof(temp_path), "%s", dir_path);
    len = strlen(temp_path);

    // Remove the trailing slash if it exists
    if (temp_path[len - 1] == '/') {
        temp_path[len - 1] = 0;
    }

    // Iterate through the path, creating directories as needed
    for (ptr = temp_path + 1; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = 0;
            // Attempt to create the directory
            if (mkdir(temp_path, 0775) != 0 && errno != EEXIST) {
                return -1;  // Return -1 on failure (except if the directory already exists)
            }
            *ptr = '/'; // Restore the '/' and continue
        }
    }

    // Create the final directory
    if (mkdir(temp_path, 0755) != 0 && errno != EEXIST) {
        return -1;  // Return -1 on failure (except if the directory already exists)
    }

    // Set permissions explicitly after creation
    if (chmod(temp_path, mode) == -1) {
        perror("Error setting directory permissions");
        return -1;
    }

    return 0;  // Return 0 on success
}

// Function to copy a directory from src to dest, handling all types of entries within.
void copy_directory(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    DIR *dir = opendir(src);
    if (!dir) {
        perror("Error opening source directory");
        return;
    }

    struct stat source_stat;
    if (lstat(src, &source_stat) == -1) {
        perror("Error getting source directory information");
        closedir(dir);
        return;
    }

    // Check if the target directory exists
    if (directory_exists(dest)) {
        errno = EEXIST; // Set errno to EEXIST to indicate that the file exists
        perror("Error: Destination directory already exists");
        closedir(dir);
        return;
    }


    // Use the source directory's permissions if copy_permissions is enabled, otherwise use 0755
    mode_t mode = copy_permissions ? source_stat.st_mode : 0755;

    // Create the target directory if it doesn't exist
    if (create_directory_recursive(dest, mode) != 0) {
        perror("Error creating target directory");
        closedir(dir);
        return;
    }

    struct dirent *entry;
    struct stat statbuf;
    char source_path[PATH_MAX];
    char target_path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        // Skip special entries "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full source and target paths
        snprintf(source_path, sizeof(source_path), "%s/%s", src, entry->d_name);
        snprintf(target_path, sizeof(target_path), "%s/%s", dest, entry->d_name);

        // Get information about the source entry
        if (lstat(source_path, &statbuf) == -1) {
            perror("Error getting source entry information");
            continue;
        }

        // Handle all types of entries (regular files, directories, symbolic links)
        if (S_ISDIR(statbuf.st_mode)) {
            // Recursive call to duplicate subdirectory
            copy_directory(source_path, target_path, copy_symlinks, copy_permissions);
        } else {
            // Duplicate regular files and symbolic links
            copy_file(source_path, target_path, copy_symlinks, copy_permissions);
        }
    }

    closedir(dir);
}
