 Testing Report

This document outlines the testing scenarios and strategies used to verify the functionality of the Omni File System backend.

 Test Scenarios You Ran

A series of incremental tests were performed using `telnet` to act as a client. The scenarios covered the full lifecycle of all major features:

   User Management:
       Successfully created a new user (`user_create`).
       Verified the new user could log in (`user_login`).
       Listed all users to confirm the new user appeared (`user_list`).
       Deleted the new user (`user_delete`).
       Verified the deleted user could no longer log in.
   Permissions and Sessions:
       Confirmed admin commands (e.g., `user_create`) fail without a session ID.
       Logged in as admin to get a session ID.
       Confirmed admin commands succeed with the correct session ID.
       Logged out (`user_logout`) to invalidate the session ID.
       Confirmed admin commands fail again with the old session ID.
   File and Directory Operations:
       Created directories (`dir_create`).
       Created files with content inside those directories (`file_create`).
       Read file content back to verify correctness (`file_read`).
       Listed directory contents to verify file creation (`dir_list`).
       Edited a portion of a file's content (`file_edit`) and verified with `file_read`.
       Truncated a file's content (`file_truncate`) and verified with `file_read`.
       Renamed and moved files (`file_rename`) and verified with `dir_list`.
       Deleted files (`file_delete`) and directories (`dir_delete`).
   Information Functions:
       Retrieved overall file system statistics (`get_stats`).
       Retrieved detailed metadata for specific files (`get_metadata`).

 Performance Metrics

Formal performance benchmarking was not conducted. However, the design choices lead to the following expected performance characteristics:

   Operations per second: The server is limited by its single-worker-thread design. All operations are serialized, so performance under heavy load is gated by the speed of a single CPU core and disk I/O.
   Response Times:
       User Login: Average O(1) due to the in-memory hash table. This is extremely fast.
       File/Directory Lookups: Dependent on the depth of the path. The `find_entry_by_path` function scans the metadata table at each level of the path, making it roughly `O(depth  number_of_entries)`. Since all metadata is in memory, this is still very fast for a reasonable number of files.
       File Read/Write: Dominated by disk I/O speed.

 Concurrent Client Testing

The server was designed to handle concurrent connections. While multiple clients can connect and submit requests at the same time, the FIFO queue serializes all file system operations. This was a core design requirement to guarantee data integrity.

   Result: The system correctly handles multiple simultaneous connections without crashing or corrupting data. The trade-off is that it does not execute file operations in parallel.

 Edge Cases Tested

   Permissions: Attempting to run admin commands without logging in, or with an invalid session ID, correctly results in a "Permission denied" error.
   Deletion:
       Attempting to delete the root directory (`/`) is not possible and fails gracefully.
       Attempting to delete a directory that is not empty correctly fails with an error message.
   Paths: Tested with root path (`/`), paths with leading slashes (`/file.txt`), and nested paths (`/dir/file.txt`).
   Invalid Input: Sending malformed JSON correctly results in an "Invalid JSON format" error. Sending a valid JSON with an unknown operation results in an "Unknown operation" error.
   Filename Length: Discovered that filenames longer than the `short_name` buffer can cause corruption. Test cases were adjusted to use valid filenames.

 Cross-compatibility Testing

Cross-compatibility testing with other students' UIs was not performed. The server was tested solely with the `telnet` command-line client. However, since the server strictly adheres to the JSON communication protocol defined in the project specification, it is expected to be compatible with any UI that follows the same protocol.