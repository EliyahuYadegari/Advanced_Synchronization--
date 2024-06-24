#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define LOCK_FILE "lockfile.lock"
#define OUTPUT_FILE "output2.txt"

// write the message "count" time
void write_message(const char *message, int count) {
    for (int i = 0; i < count; i++) {
        printf("%s\n", message);
        if (usleep((rand() % 100) * 1000) == -1) { // Random delay between 0 and 99 milliseconds
            perror("usleep");
            exit(EXIT_FAILURE);
        }
    }
}

// creat lockfile if needed, waits else
void acquire_lock() {
    while (open(LOCK_FILE, O_CREAT | O_EXCL, 0444) == -1) {
        if (errno != EEXIST) {
            perror("Failed to acquire lock");
            exit(EXIT_FAILURE);
        }
        if (usleep(100000) == -1) { // Wait 100 milliseconds before retrying
            perror("usleep");
            exit(EXIT_FAILURE);
        }
    }
}

// delete the lockfile if exist
void release_lock() {
    if (unlink(LOCK_FILE) == -1) {
        perror("Failed to release lock");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <message1> <message2> ... <count>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int count = atoi(argv[argc - 1]);
    int num_messages = argc - 2;

    srand(time(NULL));

    pid_t pids[num_messages];
    for (int i = 0; i < num_messages; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            return EXIT_FAILURE;
        } else if (pids[i] == 0) {
            // creat lockfile if needed, waits else
            acquire_lock();
            // write the message "count" time
            write_message(argv[i+1], count);
            // delete the lockfile if exist
            release_lock();
            // exit child procces
            exit(EXIT_SUCCESS);        }
    }

    // parent waits for all children to finish
    for (int i = 0; i < num_messages; i++) {
        if (waitpid(pids[i], NULL, 0) < 0) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
