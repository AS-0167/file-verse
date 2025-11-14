# Omni File System (OFS) - User Guide

This guide explains how to compile, run, and interact with the OFS server.

## 1. How to Compile and Run the Server

### Prerequisites
*   A C compiler (like `gcc`)
*   The `make` build tool
*   The `json-c` library
*   Standard C build tools (`pthreads`, etc.)

### Compilation
Navigate to the root directory of the project (`OFS_PROJECT/`) and run the `make` command:

```bash
make
This will compile all the source code and create an executable file in the compiled/ directory.
Formatting the Filesystem
Before running the server for the first time, you may need to create a fresh .omni container file. This can be done with a make command:
code
Bash
make format
This will create a new sample.omni file in the root directory. Warning: Running this command will delete any existing sample.omni file and all data within it.
Running the Server
To start the server, run the following command from the root directory:
code
Bash
make run```
By default, the server will listen on port `8080`. You should see a message confirming that the server has started:
Server listening on port 8080
code
Code
## 2. How to Use the API (with `curl`)

The server is controlled by sending JSON objects in HTTP POST requests. The `curl` command-line tool is a simple way to do this.

**All commands require a `session_id` after logging in.**

### Step 1: Login
To begin, you must log in to get a session ID. The default administrator account is `admin` with the password `admin123`.

```bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "user_login", "parameters": {"username": "admin", "password": "admin123"}}' http://localhost:8080
Response:
code
JSON
{"status":"success","data":{"session_id":"admin_17631XXXXXX"}}
You must copy the session_id value from the response to use in all subsequent commands.
Step 2: Sample File Operations
Here are examples of common file system operations. Replace YOUR_SESSION_ID with the ID you received from logging in.
Create a Directory (dir_create)
code
Bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "dir_create", "session_id": "YOUR_SESSION_ID", "parameters": {"path": "/docs"}}' http://localhost:8080
List Directory Contents (dir_list)
code
Bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "dir_list", "session_id": "YOUR_SESSION_ID", "parameters": {"path": "/"}}' http://localhost:8080
Create a File (file_create)
code
Bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "file_create", "session_id": "YOUR_SESSION_ID", "parameters": {"path": "/docs/readme.txt", "data": "Hello World"}}' http://localhost:8080
Read a File (file_read)
code
Bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "file_read", "session_id": "YOUR_SESSION_ID", "parameters": {"path": "/docs/readme.txt"}}' http://localhost:8080
Delete a File (file_delete)
code
Bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "file_delete", "session_id": "YOUR_SESSION_ID", "parameters": {"path": "/docs/readme.txt"}}' http://localhost:8080
Step 3: Logout
When you are finished, you can invalidate your session.
code
Bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "user_logout", "session_id": "YOUR_SESSION_ID"}' http://localhost:8080
3. Configuration Options
The server can be configured by editing the default.uconf file in the root directory. This file defines parameters for the filesystem, security, and server behavior. The most common configuration to change is the server port.
