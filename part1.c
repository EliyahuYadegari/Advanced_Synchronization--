#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

void child_process_1(char *child1_message, int num, FILE *output) {
    for (int i = 1; i <= num; i++) {
        fprintf(output, "%s\n", child1_message);
        
    }
    exit(0);
}

void child_process_2(char *child2_message, int num, FILE *output) {
    for (int i = 1; i <= num; i++) {
        fprintf(output, "%s\n", child2_message);
    }
    exit(0);
}

void parent_process(char *parent_message, int num, FILE *output) {
    for (int i = 1; i <= num; i++) {
        fprintf(output, "%s\n", parent_message);
    }
}


int main(int argc, char** argv) {
    // Check if the number of arguments is correct (program name + 4 arguments)
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <parent_message> <child1_message> <child2_message> <count>", argv[0]);
        return 1;
    }

    // Save each argument in a different variable
    char *parent_message = argv[1];
    char *child1_message = argv[2];
    char *child2_message = argv[3];
    int number = atoi(argv[4]); // Convert the last argument to an integer

    FILE *output_file;
    output_file = fopen("output.txt", "w");

    pid_t child1, child2;

    // Create the first child process
    child1 = fork();
    if (child1 < 0) {
        // Fork failed
        perror("fork failed");
        exit(1);
    } else if (child1 == 0) {
        // Inside the first child process
        child_process_1(child1_message, number, output_file);
    }

    // Create the second child process
    child2 = fork();
    if (child2 < 0) {
        // Fork failed
        perror("fork");
        exit(1);
    } else if (child2 == 0) {
        // Inside the second child process
        child_process_2(child2_message, number, output_file);
    }

    // Parent process waits for both children to finish
    int status;
    waitpid(child1, &status, 0);
    waitpid(child2, &status, 0);

    parent_process(parent_message, number, output_file);

    fclose(output_file)

    return 0;
}
