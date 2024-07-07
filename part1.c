#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

void replace_newline(char *str) {
    char *pos;
    while ((pos = strstr(str, "\\n")) != NULL) {
        memmove(pos, pos + 2, strlen(pos + 2) + 1); // Remove the '\\n'
    }
}

void child_process(char *message, int num, FILE *output) {
    replace_newline(message); // Remove any '\\n'
    for (int i = 0; i < num; i++) {
        fprintf(output, "%s\n", message); // Print message followed by newline
    }
    exit(0);
}

void parent_process(char *message, int num, FILE *output) {
    replace_newline(message); // Remove any '\\n'
    for (int i = 0; i < num; i++) {
        fprintf(output, "%s\n", message); // Print message followed by newline
    }
}

int main(int argc, char** argv) {
    // Check if the number of arguments is correct (program name + 4 arguments)
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <parent_message> <child1_message> <child2_message> <count>\n", argv[0]);
        return 1;
    }

    // Save each argument in a different variable
    char *parent_message = argv[1];
    char *child1_message = argv[2];
    char *child2_message = argv[3];
    int number = atoi(argv[4]); // Convert the last argument to an integer

    FILE *output_file;
    output_file = fopen("output.txt", "w");
    if (!output_file) {
        perror("fopen failed");
        return 1;
    }

    pid_t child1, child2;

    // Create the first child process
    child1 = fork();
    if (child1 < 0) {
        perror("fork failed");
        exit(1);
    } else if (child1 == 0) {
        // Inside the first child process
        child_process(child1_message, number, output_file);
    }

    // Create the second child process
    child2 = fork();
    if (child2 < 0) {
        perror("fork failed");
        exit(1);
    } else if (child2 == 0) {
        // Inside the second child process
        child_process(child2_message, number, output_file);
    }

    // Parent process waits for both children to finish
    int status;
    pid_t pid;

    pid = waitpid(child1, &status, 0);
    if (pid == -1) {
        perror("Waitpid for child1 failed");
    }

    pid = waitpid(child2, &status, 0);
    if (pid == -1) {
        perror("Waitpid for child2 failed");
    }

    parent_process(parent_message, number, output_file);

    fclose(output_file);

    return 0;
}
