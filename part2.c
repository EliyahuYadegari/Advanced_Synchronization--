#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>

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

bool file_exists(const char *filename){
    struct stat buffer;
    return stat(filename, &buffer) == 0 ? true : false;
}


int main(int argc, char** argv) {

    // Check for correct number of arguments
    if (argc < 5) {
        printf("few args");
        fprintf(stderr, "Usage: %s <message1> <message2> ... <count>\n", argv[0]);
        return 1;
    }

    // Extract messages and count from command-line arguments
    const int num_messages = argc - 2;
    const char* messages[num_messages];
    for (int i = 1; i <= num_messages; i++) {
        messages[i - 1] = argv[i];
    }
    int count = atoi(argv[argc - 1]);

    // Open lock file (create if it doesn't exist)
    const char* lockfile = "lockfile.lock";
    FILE* lock_file;
    
    // Fork child processes
    pid_t children[num_messages];
    for (int i = 0; i < num_messages; i++) {
    
    }

    for (int i = 0; i < num_messages; i++) {
        sleep(1);
        children[i] = fork();
        if (children[i] == -1) {
            perror("fork failed");
            return 1;
        } else if (children[i] == 0) {
            // Child process
            bool locked = FALSE;
            while (!locked) {
                if (!file_exists(lockfile)) {
                    // create lockfile
                    lock_file = fopen(lockfile, "w");
                    // print messege
                    write_message(messages[i], count);
                    // delete the lockfile
                    remove("lockfile.lock");
                    locked = true;
                } else {
                    if (usleep((rand() % 100) * 100) == -1) {
                        perror("usleep error");
                        exit(EXIT_FAILURE);
                    }
                    continue;
                }
            }
            exit(0); // Exit child process
        }
    }

    // Parent process waits for all children to finish
    for (int i = 0; i < num_messages; i++) {
        int status;
        if (waitpid(children[i], &status, 0) == -1) {
            perror("waitpid failed");
            return 1;
        }
    }


    return 0;
}