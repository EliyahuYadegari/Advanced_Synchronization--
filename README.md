# Buffered File I/O Library

This repository contains a C implementation of a buffered I/O library with support for a custom flag (O_PREAPPEND) that allows writing to the beginning of a file without overriding existing content. Additionally, it includes functionality similar to Python's `shutil.copytree`, enabling users to copy entire directory trees, including files and subdirectories, while handling symbolic links and file permissions.

## Installation

1. Clone this repository to your local machine and enter the project directory:

    ```bash
    cd Buffered_File_IO
    ```

2. Compile the `copytree` library:

    ```bash
    gcc -c copytree.c -o copytree.o
    ar rcs libcopytree.a copytree.o
    ```

3. Compile the main program using the copytree library:

    ```bash
    gcc part4.c -L. -lcopytree -o main_program
    ```

4. Compile the buffered I/O program:

    ```bash
    gcc -o buffered_io part3Test.c buffered_open.c
    ```

## Usage

### Running the Main Program

1. Execute the main program with the following command:

    ```bash
    ./main_program part4 part4d
    ```

    - The first argument (`part4`) is the source directory to copy.
    - The second argument (`part4d`) is the destination directory where the content will be copied.

### Running the Buffered I/O Program

1. Execute the buffered I/O program with the following command:

    ```bash
    ./buffered_io
    ```

    - This program will perform the buffered I/O operations as defined in your `part3Test.c` and `buffered_open.c` files.

## Features

- **Buffered I/O:** Efficient file handling with the ability to write to the beginning of files without losing existing content.
- **Copy Directory Trees:** Easily copy entire directory trees, preserving file permissions and handling symbolic links.
- **Customizable:** Modify and extend the library functions to suit your specific needs.
