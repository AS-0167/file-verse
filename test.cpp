#include <iostream>
#include <cstring>
#include "core/fs_core.h"
#include "core/user_manager.h"
#include "core/file_manager.h"
#include "core/dir_manager.h"

using namespace std;

const char* get_error_string(int code) {
    switch (code) {
        case 0: return "SUCCESS";
        case -1: return "ERROR_NOT_FOUND";
        case -2: return "ERROR_PERMISSION_DENIED";
        case -3: return "ERROR_IO_ERROR";
        case -4: return "ERROR_INVALID_PATH";
        case -5: return "ERROR_DIRECTORY_NOT_EMPTY";
        default: return "UNKNOWN_ERROR";
    }
}

void print_test(const char* name, int status, bool should_succeed = true) {
    cout << "[" << name << "] Status: " << status
         << " (" << get_error_string(status) << ")";
    if ((should_succeed && status == 0) || (!should_succeed && status != 0))
        cout << " ✓ PASS" << endl;
    else
        cout << " ✗ FAIL" << endl;
}

int main() {
    void* fs_instance = nullptr;
    int status;

    cout << "========================================\n";
    cout << "   FILE + DIRECTORY HIERARCHY TEST\n";
    cout << "========================================\n\n";

    // --------------------------
    // 1. Initialize File System
    // --------------------------
    cout << "--- PHASE 1: FS Setup ---" << endl;
    status = fs_format("hierarchy_test.omni", "default.uconf");
    print_test("FS Format", status);

    status = fs_init(&fs_instance, "hierarchy_test.omni", "default.uconf");
    print_test("FS Init", status);
    if (status != 0) return -1;

    FSInstance* fs = static_cast<FSInstance*>(fs_instance);
    user_manager um(fs->users);
    file_manager fm(fs, &um);
    dir_manager dm(fs->root, &um);

    // --------------------------
    // 2. User Setup
    // --------------------------
    cout << "\n--- PHASE 2: User Setup ---" << endl;
    void* admin_session = nullptr;
    status = um.user_login(&admin_session, "admin", "admin123");
    print_test("Admin Login", status);

    // --------------------------
    // 3. Create root-level file f1.txt
    // --------------------------
    cout << "\n--- PHASE 3: Root File Creation ---" << endl;
    const char* root_file = "/f1.txt";
    const char* root_data = "Hello I am F1";
    status = fm.file_create(admin_session, root_file, root_data, strlen(root_data));
    print_test("Create /f1.txt", status);

    // Verify by reading
    char* buffer = nullptr;
    size_t size = 0;
    status = fm.file_read(admin_session, root_file, &buffer, &size);
    print_test("Read /f1.txt", status);
    if (status == 0 && buffer) {
        cout << "  Content: \"" << string(buffer, size) << "\"" << endl;
        delete[] buffer;
    }

    // --------------------------
    // 4. Create directory dir1
    // --------------------------
    cout << "\n--- PHASE 4: Directory Creation ---" << endl;
    status = dm.dir_create(admin_session, "/dir1");
    print_test("Create /dir1", status);

    // List root contents
    FileEntry* entries = nullptr;
    int count = 0;
    status = dm.dir_list(admin_session, "/", &entries, &count);
    print_test("List /", status);
    if (status == 0 && entries && count > 0) {
        cout << "Root contents:" << endl;
        for (int i = 0; i < count; ++i)
            cout << "  - " << entries[i].name
                 << (entries[i].getType() == EntryType::DIRECTORY ? " [DIR]" : " [FILE]")
                 << endl;
        delete[] entries;
    }

    // --------------------------
    // 5. Create file inside dir1
    // --------------------------
    cout << "\n--- PHASE 5: Nested File Creation ---" << endl;
    const char* nested_file = "/dir1/f1.txt";
    const char* nested_data = "Hello I am F1 under Dir 1";
    status = fm.file_create(admin_session, nested_file, nested_data, strlen(nested_data));
    print_test("Create /dir1/f1.txt", status);

    // Verify nested file content
    buffer = nullptr; size = 0;
    status = fm.file_read(admin_session, nested_file, &buffer, &size);
    print_test("Read /dir1/f1.txt", status);
    if (status == 0 && buffer) {
        cout << "  Content: \"" << string(buffer, size) << "\"" << endl;
        delete[] buffer;
    }

    // --------------------------
    // 6. List contents of /dir1
    // --------------------------
    cout << "\n--- PHASE 6: List /dir1 ---" << endl;
    entries = nullptr; count = 0;
    status = dm.dir_list(admin_session, "/dir1", &entries, &count);
    print_test("List /dir1", status);
    if (status == 0 && entries && count > 0) {
        cout << "Contents of /dir1:" << endl;
        for (int i = 0; i < count; ++i)
            cout << "  - " << entries[i].name
                 << (entries[i].getType() == EntryType::DIRECTORY ? " [DIR]" : " [FILE]")
                 << endl;
        delete[] entries;
    }

    // --------------------------
    // 7. Clean-up Deletion
    // --------------------------
    cout << "\n--- PHASE 7: Cleanup ---" << endl;

    // Delete nested file
    status = fm.file_delete(admin_session, nested_file);
    print_test("Delete /dir1/f1.txt", status);

    // Delete empty directory
    status = dm.dir_delete(admin_session, "/dir1");
    
    print_test("Delete /dir1", status);

    // Delete root file
    status = fm.file_delete(admin_session, root_file);
    print_test("Delete /f1.txt", status);

    // --------------------------
    // 8. Final Cleanup
    // --------------------------
    cout << "\n--- PHASE 8: Shutdown ---" << endl;
    um.user_logout(admin_session);
    cout << "Admin logged out." << endl;

    fs_shutdown(fs_instance);
    cout << "FS shutdown completed." << endl;

    cout << "\n========================================" << endl;
    cout << "  FILE + DIRECTORY HIERARCHY TEST DONE" << endl;
    cout << "========================================" << endl;

    return 0;
}



//g++ -std=c++17 -Isource/include -Isource/include/core source/core/*.cpp source/*.cpp test.cpp -o test_ofs -lssl -lcrypto

