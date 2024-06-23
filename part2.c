#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define TRUE 1
#define FALSE 0

void write_message(const char *message, int count) {
    for (int i = 0; i < count; i++) {
        printf("%s\n", message);
        if (usleep((rand() % 100) * 1000) == -1) {
            perror("usleep error");
            exit(EXIT_FAILURE);
        }
    }
}

int getlock(const char *lockfile) {
    // Check if lock file exists
    return access(lockfile, F_OK);
}

int setlock(const char *lockfile) {
    int fd = open(lockfile, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("Failed to create lock file");
        return -1;
    }
    // Implement locking mechanism (e.g., flock, fcntl, etc.)
    // Example: flock(fd, LOCK_EX);
    return 0;
}

int main(int argc, char** argv) {

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <message1> <message2> ... <count>\n", argv[0]);
        return 1;
    }

    int num_children = argc - 2; // Calculate number of children
    pid_t *children = malloc(num_children * sizeof(pid_t));

    if (children == NULL) {
        perror("Failed to allocate memory for children array");
        return 1;
    }

    for (int i = 1; i <= num_children; i++) {
        children[i - 1] = fork();

        if (children[i - 1] < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (children[i - 1] == 0) {
            // Inside the child process
            bool locked = FALSE;
            while (!locked) {
                if (getlock(lockfile) == 0) {
                    // Lock file is not present, create lock file and acquire lock
                    int lockstatus = setlock(lockfile);
                    if (lockstatus == 0) {
                        write_message(argv[i], atoi(argv[argc - 1]));
                        // Release the lock
                        unlink(lockfile);
                        locked = TRUE;
                    }
                } else {
                    if (usleep((rand() % 100) * 1000) == -1) {
                        perror("usleep error");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            exit(EXIT_SUCCESS); // Exit child process
        }
    }

    // Parent process waits for all children to finish
    for (int i = 0; i < num_children; i++) {
        int status;
        pid_t pid = waitpid(children[i], &status, 0);
        if (pid == -1) {
            perror("Waitpid for child failed");
        }
    }



    free(children); // Free allocated memory

    return 0;
}
