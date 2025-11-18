#include <iostream>
#include <iomanip>
#include <cstring>
#include <ctime>
#include "core/fs_core.h"
#include "core/user_manager.h"
#include "core/file_manager.h"
#include "core/dir_manager.h"
#include "core/metadata.h"
#include "odf_types.hpp"

using namespace std;

// ============================================================================
// Utility Functions
// ============================================================================
const char* get_error_string(int code) {
    switch (code) {
        case 0: return "SUCCESS";
        case -1: return "ERROR_NOT_FOUND";
        case -2: return "ERROR_PERMISSION_DENIED";
        case -3: return "ERROR_IO_ERROR";
        case -4: return "ERROR_INVALID_PATH";
        case -5: return "ERROR_FILE_EXISTS";
        case -6: return "ERROR_NO_SPACE";
        case -7: return "ERROR_INVALID_CONFIG";
        case -8: return "ERROR_NOT_IMPLEMENTED";
        case -9: return "ERROR_INVALID_SESSION";
        case -10: return "ERROR_DIRECTORY_NOT_EMPTY";
        case -11: return "ERROR_INVALID_OPERATION";
        default: return "UNKNOWN_ERROR";
    }
}

void print_test(const string& test_name, int status) {
    const char* green = "\033[32m";
    const char* red = "\033[31m";
    const char* reset = "\033[0m";

    cout << left << setw(45) << test_name << " : ";
    if (status == 0)
        cout << green << "PASS" << reset << endl;
    else
        cout << red << "FAIL (" << get_error_string(status) << ")" << reset << endl;
}

// ============================================================================
// MAIN TEST HARNESS
// ============================================================================
int main() {
    cout << "\n==============================\n";
    cout << "   🌐 OFS CORE FUNCTIONAL TEST\n";
    cout << "==============================\n\n";

    FSInstance* fs = nullptr;
    int status;

    // ------------------------------------------------------------------------
    // Step 1: Format & Initialize File System
    // ------------------------------------------------------------------------
    status = fs_format("student_test.omni", "default_config.txt");
    print_test("Format FS", status);

    status = fs_init((void**)&fs, "student_test.omni", "default_config.txt");
    print_test("Initialize FS", status);

    // Build core managers
    user_manager users(fs->users);
    dir_manager dirs(fs->root, &users);
    file_manager files(fs, &users);
    metadata meta(fs);

    // ------------------------------------------------------------------------
    // Step 2: Admin Login
    // ------------------------------------------------------------------------
    void* admin_session = nullptr;
    status = users.user_login(&admin_session, "admin", "admin123");
    print_test("Admin Login", status);

    
    // ------------------------------------------------------------------------
    // Step 3: Directory Creation
    // ------------------------------------------------------------------------
    status = dirs.dir_create(admin_session, "/docs");
    print_test("Create /docs", status);

    status = dirs.dir_create(admin_session, "/docs/reports");
    print_test("Create /docs/reports", status);
    cout <<"Hey Directory No issue";
    // ------------------------------------------------------------------------
    // Step 4: File Creation and Reading
    // ------------------------------------------------------------------------
   
    const char* data = "dfbbhbnbnbnnbnnbvdfghghahggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvsddddddddddddddddddddddtrfgfgggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeefffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffgggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggghhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiijjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrsssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssstttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    status = files.file_create(admin_session, "/docs/info.txt", data, strlen(data));
    print_test("Create /docs/info.txt", status);

    char* buffer = nullptr;
    size_t size = 0;
    status = files.file_read(admin_session, "/docs/info.txt", &buffer, &size);

    print_test("Read /docs/info.txt", status);
    if (status == 0 && buffer)
        cout << "  → File content: " << string(buffer, size) << endl;

    // ------------------------------------------------------------------------
    // Step 5: Rename and Truncate
    // ------------------------------------------------------------------------
    //status = files.file_rename(admin_session, "/docs/info.txt", "/docs/info_v2.txt");
    //print_test("Rename /docs/info.txt → /docs/info_v2.txt", status);

    ///status = files.file_truncate(admin_session, "/docs/info.txt");
    //print_test("Truncate /docs/info.txt", status);


    // ------------------------------------------------------------------------
// Step 6: Metadata & Stats
// ------------------------------------------------------------------------
FileMetadata meta_info;
status = meta.get_metadata(admin_session, "/docs/info.txt", &meta_info);
print_test("Metadata for /docs/info_v2.txt", status);
if (status == 0) {
    cout << "  Owner: " << meta_info.entry.owner
         << ", Size: " << meta_info.entry.size
         << ", Type: " << (meta_info.entry.getType() == EntryType::FILE ? "File" : "Dir")
         <<", Blocks Used: "<<meta_info.blocks_used
         <<", Permissions: "<<meta_info.entry.permissions<<
          endl;
}

FSStats stats;
status = meta.get_stats(admin_session, &stats);
print_test("FS Stats 1", status);
if (status == 0)
    cout << "  Files: " << stats.total_files 
         << ", Dirs: " << stats.total_directories
         << ", Users: " << stats.total_users << endl;

// ---------------------------
// DEBUG: Print all users in the hash table
// ---------------------------
cout << "[DEBUG] Users in system after Step 6:" << endl;
if (fs->users) {
    for (auto head : fs->users->getBuckets()) {
        auto node = head;
        while (node) {
            if (node->value)
                cout << "  User: " << node->value->username
                     << ", Active: " << (int)node->value->is_active << endl;
            node = node->next;
        }
    }
}

// ------------------------------------------------------------------------
// Step 7: User Creation & Permissions Test
// ------------------------------------------------------------------------
status = users.user_create(admin_session, "bob", "bob123", UserRole::NORMAL);
meta.set_owner(admin_session, "/", "bob");
print_test("Create user 'bob'", status);

// DEBUG: Print users again to confirm increment
cout << "[DEBUG] Users in system after creating 'bob':" << endl;
if (fs->users) {
    for (auto head : fs->users->getBuckets()) {
        auto node = head;
        while (node) {
            if (node->value)
                cout << "  User: " << node->value->username
                     << ", Active: " << (int)node->value->is_active << endl;
            node = node->next;
        }
    }
}
uint32_t all_perms = 
    static_cast<uint32_t>(FilePermissions::OWNER_READ)    |
    static_cast<uint32_t>(FilePermissions::OWNER_WRITE)   |
    static_cast<uint32_t>(FilePermissions::OWNER_EXECUTE) |
    static_cast<uint32_t>(FilePermissions::GROUP_READ)    |
    static_cast<uint32_t>(FilePermissions::GROUP_WRITE)   |
    static_cast<uint32_t>(FilePermissions::GROUP_EXECUTE) |
    static_cast<uint32_t>(FilePermissions::OTHERS_READ)   |
    static_cast<uint32_t>(FilePermissions::OTHERS_WRITE)  |
    static_cast<uint32_t>(FilePermissions::OTHERS_EXECUTE);

meta.set_permissions(admin_session,"/",all_perms);
    // ------------------------------------------------------------------------
    // Step 7: User Creation & Permissions Test
    // ------------------------------------------------------------------------
    //status = users.user_create(admin_session, "bob", "bob123", UserRole::NORMAL);
    //meta.set_owner(admin_session, "/", "bob");

    //print_test("Create user 'bob'", status);

    void* bob_session = nullptr;
    status = users.user_login(&bob_session, "bob", "bob123");
    print_test("Bob Login", status);

    status = dirs.dir_create(bob_session, "/bob");
    print_test("Bob Create /bob", status);

    status = files.file_create(bob_session, "/bob/bob_file.txt", "Hello from Bob", 15);
    print_test("Bob Create /bob/bob_file.txt", status);

    
    data = "A";
    status = files.file_edit(admin_session, "/docs/info.txt", data, strlen(data),1);
    print_test("Admin Edit /docs/info.txt",status);

    status = meta.get_metadata(bob_session, "/bob/bob_file.txt",&meta_info);
    if (status == 0) {
    cout << "  Owner: " << meta_info.entry.owner
         << ", Size: " << meta_info.entry.size
         << ", Type: " << (meta_info.entry.getType() == EntryType::FILE ? "File" : "Dir") <<", Blocks Used: "<< meta_info.blocks_used<<
         ", Permissions: "<<meta_info.entry.permissions<< endl;
}
    buffer = nullptr;
    size = 0;
    status = files.file_read(admin_session, "/bob/bob_file.txt", &buffer, &size);

    print_test("Read /bob/bob_file.txt", status);
    if (status == 0 && buffer)
        cout << "  → File content: " << string(buffer, size) << endl;

    buffer = nullptr;
    size = 0;
     status = files.file_read(admin_session, "/docs/info.txt", &buffer, &size);

    print_test("Read /docs/info.txt", status);
    if (status == 0 && buffer)
        cout << "  → File content: " << string(buffer, size) << endl;

    status = meta.get_stats(admin_session, &stats);
print_test("FS Stats 2", status);
if (status == 0)
    cout << "  Files: " << stats.total_files 
         << ", Dirs: " << stats.total_directories
         << ", Users: " << stats.total_users << endl;
    // ------------------------------------------------------------------------
    // Step 8: Directory Listing
    // ------------------------------------------------------------------------
    FileEntry* entries = nullptr;
    int count = 0;
    status = dirs.dir_list(admin_session, "/docs", &entries, &count);
    print_test("List /docs contents", status);

    if (status == 0 && entries) {
        cout << "  Directory entries (" << count << "):\n";
        for (int i = 0; i < count; i++) {
            cout << "   - " << entries[i].name
                 << (entries[i].getType() == EntryType::DIRECTORY ? " [DIR]" : " [FILE]") << endl;
        }
    }



    // ------------------------------------------------------------------------
    // Step 9: Cleanup
    // ------------------------------------------------------------------------
    users.user_logout(admin_session);
    print_test("Admin Logout", 0);

    fs_shutdown(fs);
    print_test("Shutdown FS", 0);

    cout << "\n✅ OFS test complete.\n";
    return 0;
}


//g++ -std=c++17 -Isource/include -Isource/include/core source/data_structures/*.cpp source/core/*.cpp test.cpp -o test_ofs -lssl -lcrypto

