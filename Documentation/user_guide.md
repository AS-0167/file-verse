 User Guide

This guide explains how to compile and run the Omni File System server and how to interact with it.

 How to compile and run your server

The project uses a standard `Makefile`.

1.  Compile: Open a terminal in the root directory of the project and run the `make` command. This will compile all the source files and create an executable file named `run_ofs`.
    ```bash
    make
    ```
    To do a clean rebuild, you can run:
    ```bash
    make clean && make
    ```

2.  Run: Execute the compiled program from your terminal:
    ```bash
    ./run_ofs
    ```
    The server will start and print messages indicating it is listening for connections on `localhost`, port `8080`.

 How to use your UI

The backend is a command-line server and does not have a graphical UI. You can interact with it using any tool that can send plain text over a TCP socket. For testing, the standard `telnet` utility is recommended.

To connect to the server, open a new terminal and run:
```bash
telnet localhost 8080