# Omni File System (OFS) - Testing Report

This report documents the test scenarios executed to ensure the functionality, correctness, and stability of the OFS server.

## 1. Test Scenarios Ran

A comprehensive suite of tests was conducted using the `curl` command-line tool to simulate a client interacting with the JSON API. Each core function was tested with a sequence of commands to verify its behavior from start to finish.

### Session Management (`user_login`, `user_logout`)
A full session lifecycle was tested:
1.  **Login:** A user successfully logged in to receive a valid session ID.
2.  **Use Session:** The session ID was used to perform a valid action (`dir_list`), which succeeded.
3.  **Logout:** The `user_logout` command was called with the session ID, which succeeded.
4.  **Verify Invalidation:** The same session ID was used again to attempt another `dir_list`, which correctly failed with an "Invalid session" error.

### Admin-Only User Creation (`user_create`)
The permission model for user creation was tested:
1.  **Admin Action:** The `admin` user logged in and successfully created a new non-admin user, "brian".
2.  **New User Login:** The new user "brian" successfully logged in and received their own session ID.
3.  **Permission Denial:** "brian"'s non-admin session was used to attempt to create a third user, "charlie". This attempt correctly failed with a "Permission denied" error.
4.  **Collision Test:** The `admin` user attempted to create "brian" a second time, which correctly failed with a "User already exists" error.

### File Rename and Move (`file_rename`)
This test verified both renaming a file in place and moving it to a new directory.
1.  **Setup:** Created a file `/original.txt` and a directory `/docs`.
2.  **Test Rename:** Renamed `/original.txt` to `/renamed.txt`. Verified with `dir_list` that the change was successful.
3.  **Test Move:** Moved `/renamed.txt` to `/docs/final_v.txt`.
4.  **Verify Move:** Verified with `dir_list` that the root directory was now empty of files and that `/docs` now contained `final_v.txt`.
5.  **Verify Content:** Read the file from its new location (`/docs/final_v.txt`) to ensure its data was preserved.

*Similar multi-step, state-verifying tests were performed for all other API endpoints, including `file_create`, `file_read`, `file_edit`, `file_delete`, `file_truncate`, `dir_create`, `dir_delete`, and `get_metadata`.*

## 2. Edge Cases Tested

Several edge cases were tested to ensure server stability and correctness:

*   **Invalid Inputs:** Sending requests with missing JSON parameters (e.g., `file_create` with no `path`) resulted in the correct JSON error response.
*   **Permissions:** Attempting to perform an admin-only action (like `user_create`) or modify a file owned by another user (like `set_permissions`) correctly returned a `Permission denied` error.
*   **Non-Existent Items:** Attempting to read, delete, or get metadata for a non-existent file or path correctly returned a `File not found` error.
*   **Name Collisions:** Attempting to create a file or directory with a name that already exists in the same location correctly returned an `Already exists` error.
*   **Filename Length Limit:** Attempting to rename a file to a name longer than the 11-character on-disk limit was correctly rejected with an error.
*   **Directory Not Empty:** Attempting to delete a directory that contained files was correctly rejected with a "not empty" error.
*   **Invalid Session:** Attempting any authenticated file operation after `user_logout` correctly returned an `Invalid session` error.

## 3. Performance Metrics

Formal performance benchmarking with high-throughput tools was not conducted. Responsiveness was evaluated manually via `curl` on a local machine.

*   **Observations:** Due to the system's architecture, which loads all metadata into memory at startup, all API operations were **instantaneous**, with no perceptible latency.
*   **Architectural Reason:** The use of a `path_cache` hash table provides **O(1) time complexity** for all file and directory lookups. This ensures that performance does not degrade as the number of files or the depth of the directory structure increases. Disk I/O is only performed when absolutely necessary (reading/writing file content).

## 4. Concurrent Client Testing Results

Basic concurrency was tested manually by opening two separate terminals and sending `curl` requests in quick succession.

*   **Observations:** The server handled concurrent requests correctly without crashing or corrupting data.
*   **Architectural Reason:** The **FIFO queue and single worker thread architecture** correctly serialized all incoming requests. This design ensures that one file system operation completes entirely before the next one begins, which is a simple and effective strategy to prevent all race conditions and guarantee data consistency.

## 5. Cross-Compatibility Testing

Cross-compatibility testing with other students' user interfaces or backends was not performed as part of this development cycle.

However, the server adheres to a strict, well-defined JSON protocol over standard HTTP. Any client capable of sending and receiving JSON over a standard TCP socket should be compatible.
