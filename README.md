# Advanced Synchronization of File Access

This repository contains two C programs demonstrating different approaches to synchronizing file access between multiple processes. These programs showcase both naive methods and more advanced locking mechanisms to handle concurrent file writes, ensuring that processes do not interfere with each other's operations.

## Installation

1. Clone this repository to your local machine and navigate to the project directory:

    ```bash
    cd Advanced_Synchronization
    ```

2. Compile the C source files:

    ```bash
    gcc part1.c -o part1
    gcc part2.c -o part2
    ```

3. Set the appropriate permissions to make the compiled programs executable:

    ```bash
    chmod +x part1
    chmod +x part2
    ```

## Usage

### Part 1: Naive Synchronization

The `part1` program uses basic synchronization techniques (`wait` and `sleep`) to manage file access between parent and child processes.

1. Run the `part1` program with the following command:

    ```bash
    ./part1 "Parent message" "Child1 message" "Child2 message" 3
    ```

    - **Parent message**: The message the parent process will write.
    - **Child1 message**: The message the first child process will write.
    - **Child2 message**: The message the second child process will write.
    - **3**: The number of times each process will write its message to the file.

2. The output will be saved in a file named `output.txt` in the project directory.

### Part 2: Synchronization Lock

The `part2` program implements a synchronization lock using a lock file to control access to the shared file, ensuring only one process writes at a time.

1. Run the `part2` program with the following command:

    ```bash
    ./part2 "First message" "Second message" "Third message" 3 > output2.txt
    ```

    - **First message**, **Second message**, **Third message**: The messages that each child process will write.
    - **3**: The number of times each process will write its message to the file.

2. The output will be saved in a file named `output2.txt` in the project directory.

## Features

- **Naive Synchronization (Part 1):** Demonstrates basic synchronization using `sleep` and `wait` to prevent interleaving of file writes by parent and child processes.
- **Synchronization Lock (Part 2):** Implements a locking mechanism using a lock file to ensure mutual exclusion during file writes, allowing safe concurrent processing.
- **Command Line Interface:** Easy-to-use command line interface for executing the programs with customizable input parameters.
- **Error Handling:** Includes basic error handling for process creation, synchronization functions, and file operations.
