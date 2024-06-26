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

// Function to copy a file from src to dest, with options to handle symlinks and permissions.
void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    // Open the source file for reading
    int source_fd = open(src, O_RDONLY);
    if (source_fd == -1) {
        perror("Error opening source file");
        return;
    }

    // Open the target file for writing
    int target_fd;
    if (copy_symlinks) {
        // If copying symbolic links as links, create a symbolic link
        if (symlink(src, dest) == -1) {
            perror("Error creating symbolic link");
            close(source_fd);
            return;
        }
    } else {
        // Otherwise, open the target file for writing (copy content)
        target_fd = open(dest, O_WRONLY | O_CREAT, 0666);
        if (target_fd == -1) {
            perror("Error opening target file");
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
        // Close the target file
        close(target_fd);
    }

    // Set permissions of the target file if copy_permissions is enabled
    if (copy_permissions) {
        // Get the permissions of the source file
        struct stat source_stat;
        if (fstat(source_fd, &source_stat) == -1) {
            perror("Error getting source file permissions");
            close(source_fd);
            return;
        }
        // Close the source file
        close(source_fd);

        // Set the permissions of the target file
        if (chmod(dest, source_stat.st_permissions) == -1) {
            perror("Error setting target file permissions");
            return;
        }
    } else {
        // Close the source file if permissions are not copied
        close(source_fd);
    }
}
// Helper function to create directories recursively
int create_directory_recursive(const char *dir_path, permissions_t permissions) {
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
    if (chmod(temp_path, permissions) == -1) {
        perror("Error setting directory permissions");
        return -1;
    }

    // Print confirmation of directory creation with permissions
    struct stat st;
    if (lstat(temp_path, &st) == 0) {
        printf("Directory %s created with %o permissions\n", temp_path, st.st_permissions);
    } else {
        perror("Error confirming directory creation");
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

    // Use the source directory's permissions if copy_permissions is enabled, otherwise use 0755
    permissions_t permissions = copy_permissions ? source_stat.st_permissions : 0755;

    // Create the target directory if it doesn't exist
    if (create_directory_recursive(dest, permissions) != 0) {
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
        if (S_ISDIR(statbuf.st_permissions)) {
            // Recursive call to duplicate subdirectory
            copy_directory(source_path, target_path, copy_symlinks, copy_permissions);
        } else {
            // Duplicate regular files and symbolic links
            copy_file(source_path, target_path, copy_symlinks, copy_permissions);
        }
    }

    closedir(dir);
}
