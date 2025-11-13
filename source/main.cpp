#include "ofs_core.hpp"
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>

using namespace std;

// Global OFS instance and session
OFSCore* g_ofs = nullptr;
void* g_session = nullptr;
string g_current_user;
UserRole g_current_role;

void clearScreen() {
    cout << "\033[2J\033[1;1H";
}

void printBanner() {
    cout << "============================================================\n";
    cout << "         OFS - Omni File System (Phase 1)                  \n";
    cout << "         Student ID: BSCS24115                             \n";
    cout << "============================================================\n";
    cout << endl;
}

void printMenu() {
    cout << "\n--- Main Menu -------------------------------------------\n";
    cout << "User: " << g_current_user;
    cout << " (" << (g_current_role == UserRole::ADMIN ? "ADMIN" : "NORMAL") << ")\n";
    cout << "---------------------------------------------------------\n";
    
    cout << "\nFile Operations:\n";
    cout << "  1. Create File\n";
    cout << "  2. Read File\n";
    cout << "  3. Edit File\n";
    cout << "  4. Delete File\n";
    cout << "  5. Rename File\n";
    
    cout << "\nDirectory Operations:\n";
    cout << "  6. Create Directory\n";
    cout << "  7. List Directory\n";
    cout << "  8. Delete Directory\n";
    
    cout << "\nInformation:\n";
    cout << "  9. Show File/Dir Info\n";
    cout << " 10. Show Statistics\n";
    cout << " 11. Show Session Info\n";
    
    if (g_current_role == UserRole::ADMIN) {
        cout << "\nAdmin Only:\n";
        cout << " 12. Create User\n";
        cout << " 13. Delete User\n";
        cout << " 14. List All Users\n";
        cout << " 15. Format File System (WARNING: Erases all data!)\n";
    }
    
    cout << "\nSystem:\n";
    cout << " 16. Change Permissions\n";
    cout << " 17. Logout\n";
    cout << "  0. Exit\n";
    
    cout << "---------------------------------------------------------\n";
    cout << "\nChoice: ";
}

string getInput(const string& prompt) {
    string input;
    cout << prompt;
    getline(cin, input);
    return input;
}

void pressEnter() {
    cout << "\nPress ENTER to continue...";
    if (cin.fail()) {
        cin.clear();
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

void printSuccess(const string& msg) {
    cout << "[SUCCESS] " << msg << endl;
}

void printError(const string& msg) {
    cout << "[ERROR] " << msg << endl;
}

void printInfo(const string& msg) {
    cout << "[INFO] " << msg << endl;
}

void printWarning(const string& msg) {
    cout << "[WARNING] " << msg << endl;
}

// Menu Functions

void createFile() {
    clearScreen();
    cout << "=== Create File ===\n" << endl;
    
    string path = getInput("Enter file path (e.g., /documents/file.txt): ");
    if (path.empty() || path[0] != '/') {
        printError("Invalid path! Must start with /");
        pressEnter();
        return;
    }
    
    cout << "Enter file content (type 'EOF' on a new line when done):\n";
    string content, line;
    while (getline(cin, line)) {
        if (line == "EOF") break;
        content += line + "\n";
    }
    
    if (cin.eof()) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    
    int result = g_ofs->fileCreate(g_session, path.c_str(), content.c_str(), content.length());
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("File created: " + path);
        printInfo("Size: " + to_string(content.length()) + " bytes");
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void readFile() {
    clearScreen();
    cout << "=== Read File ===\n" << endl;
    
    string path = getInput("Enter file path: ");
    
    char* buffer;
    size_t size;
    int result = g_ofs->fileRead(g_session, path.c_str(), &buffer, &size);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\n--- Content (" << size << " bytes) ---" << endl;
        cout << string(buffer, size);
        cout << "\n--- End of Content ---" << endl;
        g_ofs->freeBuffer(buffer);
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void editFile() {
    clearScreen();
    cout << "=== Edit File ===\n" << endl;
    
    string path = getInput("Enter file path: ");
    string offsetStr = getInput("Enter offset to edit at: ");
    
    cout << "Enter new content (single line):\n";
    string content;
    getline(cin, content);
    
    uint32_t offset = stoul(offsetStr);
    int result = g_ofs->fileEdit(g_session, path.c_str(), content.c_str(), content.length(), offset);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("File edited successfully");
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void deleteFile() {
    clearScreen();
    cout << "=== Delete File ===\n" << endl;
    
    string path = getInput("Enter file path: ");
    string confirm = getInput("Are you sure? (yes/no): ");
    
    if (confirm != "yes") {
        printInfo("Cancelled");
        pressEnter();
        return;
    }
    
    int result = g_ofs->fileDelete(g_session, path.c_str());
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("File deleted: " + path);
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void renameFile() {
    clearScreen();
    cout << "=== Rename File ===\n" << endl;
    
    string oldPath = getInput("Enter current path: ");
    string newPath = getInput("Enter new path: ");
    
    int result = g_ofs->fileRename(g_session, oldPath.c_str(), newPath.c_str());
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("File renamed successfully");
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void createDirectory() {
    clearScreen();
    cout << "=== Create Directory ===\n" << endl;
    
    string path = getInput("Enter directory path (e.g., /documents): ");
    
    int result = g_ofs->dirCreate(g_session, path.c_str());
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("Directory created: " + path);
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void listDirectory() {
    clearScreen();
    cout << "=== List Directory ===\n" << endl;
    
    string path = getInput("Enter directory path (/ for root): ");
    
    FileEntry* entries;
    int count;
    int result = g_ofs->dirList(g_session, path.c_str(), &entries, &count);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\nDirectory: " << path << endl;
        cout << "Found " << count << " entries:\n" << endl;
        
        cout << left << setw(30) << "Name" << setw(10) << "Type" << setw(15) << "Size" << "Permissions" << endl;
        cout << string(70, '-') << endl;
        
        for (int i = 0; i < count; i++) {
            string type = (entries[i].type == static_cast<uint8_t>(EntryType::DIRECTORY)) ? 
                         "[DIR]" : "[FILE]";
            
            cout << left << setw(30) << entries[i].name 
                 << setw(10) << type
                 << setw(15) << entries[i].size
                 << oct << entries[i].permissions << dec
                 << endl;
        }
        
        delete[] entries;
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void deleteDirectory() {
    clearScreen();
    cout << "=== Delete Directory ===\n" << endl;
    
    string path = getInput("Enter directory path: ");
    string confirm = getInput("Are you sure? Directory must be empty! (yes/no): ");
    
    if (confirm != "yes") {
        printInfo("Cancelled");
        pressEnter();
        return;
    }
    
    int result = g_ofs->dirDelete(g_session, path.c_str());
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("Directory deleted: " + path);
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void showInfo() {
    clearScreen();
    cout << "=== File/Directory Information ===\n" << endl;
    
    string path = getInput("Enter path: ");
    
    FileMetadata meta;
    int result = g_ofs->getMetadata(g_session, path.c_str(), &meta);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\n--- Metadata ---" << endl;
        cout << "Path:        " << meta.path << endl;
        cout << "Name:        " << meta.entry.name << endl;
        cout << "Type:        " << (meta.entry.type ? "Directory" : "File") << endl;
        cout << "Size:        " << meta.entry.size << " bytes" << endl;
        cout << "Blocks Used: " << meta.blocks_used << endl;
        cout << "Actual Size: " << meta.actual_size << " bytes" << endl;
        cout << "Permissions: " << oct << meta.entry.permissions << dec << endl;
        cout << "Inode:       " << meta.entry.inode << endl;
        cout << "Owner:       " << meta.entry.owner << endl;
        cout << "----------------" << endl;
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void showStatistics() {
    clearScreen();
    cout << "=== File System Statistics ===\n" << endl;
    
    FSStats stats;
    int result = g_ofs->getStats(g_session, &stats);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\n--- Statistics ---" << endl;
        cout << "Total Size:       " << stats.total_size / (1024*1024) << " MB" << endl;
        cout << "Used Space:       " << stats.used_space / 1024 << " KB" << endl;
        cout << "Free Space:       " << stats.free_space / (1024*1024) << " MB" << endl;
        cout << "Total Files:      " << stats.total_files << endl;
        cout << "Total Directories:" << stats.total_directories << endl;
        cout << "Total Users:      " << stats.total_users << endl;
        cout << "Active Sessions:  " << stats.active_sessions << endl;
        cout << "Fragmentation:    " << fixed << setprecision(2) << stats.fragmentation << "%" << endl;
        cout << "------------------" << endl;
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void showSessionInfo() {
    clearScreen();
    cout << "=== Session Information ===\n" << endl;
    
    SessionInfo info;
    int result = g_ofs->getSessionInfo(g_session, &info);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "\n--- Session ---" << endl;
        cout << "Session ID:       " << info.session_id << endl;
        cout << "Username:         " << info.user.username << endl;
        cout << "Role:             " << (info.user.role == UserRole::ADMIN ? "ADMIN" : "NORMAL") << endl;
        cout << "Login Time:       " << info.login_time << endl;
        cout << "Last Activity:    " << info.last_activity << endl;
        cout << "Operations Count: " << info.operations_count << endl;
        cout << "---------------" << endl;
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void createUser() {
    if (g_current_role != UserRole::ADMIN) {
        printError("Admin access required!");
        pressEnter();
        return;
    }
    
    clearScreen();
    cout << "=== Create User (Admin) ===\n" << endl;
    
    string username = getInput("Enter username: ");
    string password = getInput("Enter password: ");
    string roleStr = getInput("Enter role (admin/normal): ");
    
    UserRole role = (roleStr == "admin") ? UserRole::ADMIN : UserRole::NORMAL;
    
    int result = g_ofs->userCreate(g_session, username.c_str(), password.c_str(), role);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("User created: " + username);
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void deleteUser() {
    if (g_current_role != UserRole::ADMIN) {
        printError("Admin access required!");
        pressEnter();
        return;
    }
    
    clearScreen();
    cout << "=== Delete User (Admin) ===\n" << endl;
    
    string username = getInput("Enter username to delete: ");
    string confirm = getInput("Are you sure? (yes/no): ");
    
    if (confirm != "yes") {
        printInfo("Cancelled");
        pressEnter();
        return;
    }
    
    int result = g_ofs->userDelete(g_session, username.c_str());
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("User deleted: " + username);
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void listUsers() {
    if (g_current_role != UserRole::ADMIN) {
        printError("Admin access required!");
        pressEnter();
        return;
    }
    
    clearScreen();
    cout << "=== List All Users (Admin) ===\n" << endl;
    
    UserInfo* users;
    int count;
    int result = g_ofs->userList(g_session, &users, &count);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        cout << "Total users: " << count << "\n" << endl;
        
        cout << left << setw(20) << "Username" << setw(15) << "Role" << setw(20) << "Created" << setw(20) << "Last Login" << endl;
        cout << string(75, '-') << endl;
        
        for (int i = 0; i < count; i++) {
            string role = (users[i].role == UserRole::ADMIN) ? "ADMIN" : "NORMAL";
            cout << left << setw(20) << users[i].username
                 << setw(15) << role
                 << setw(20) << users[i].created_time
                 << setw(20) << users[i].last_login
                 << endl;
        }
        
        delete[] users;
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

void formatFileSystem() {
    if (g_current_role != UserRole::ADMIN) {
        printError("Admin access required!");
        pressEnter();
        return;
    }
    
    clearScreen();
    cout << "=== Format File System (Admin) ===\n" << endl;
    
    printWarning("!!! DANGER !!!");
    printWarning("This will ERASE ALL DATA in the file system!");
    printWarning("All files, directories, and users (except admin) will be DELETED!");
    cout << endl;
    
    string confirm1 = getInput("Are you absolutely sure? Type 'YES' to confirm: ");
    if (confirm1 != "YES") {
        printInfo("Format cancelled - no changes made");
        pressEnter();
        return;
    }
    
    string confirm2 = getInput("Last warning! Type 'FORMAT' to proceed: ");
    if (confirm2 != "FORMAT") {
        printInfo("Format cancelled - no changes made");
        pressEnter();
        return;
    }
    
    cout << "\nFormatting file system..." << endl;
    
    // Logout current user
    g_ofs->userLogout(g_session);
    g_session = nullptr;
    
    // Shutdown current instance
    g_ofs->shutdown();
    
    // Format the file system
    const char* omni_path = "BSCS24115.omni";
    const char* config_path = "compiled/default.uconf";
    
    int result = g_ofs->format(omni_path, config_path);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("File system formatted successfully!");
        cout << "\nDefault admin account recreated:" << endl;
        cout << "  Username: admin" << endl;
        cout << "  Password: admin123" << endl;
        
        // Re-initialize
        result = g_ofs->initialize(omni_path, config_path);
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            printSuccess("File system re-initialized");
            printInfo("Please login again to continue");
        } else {
            printError("Failed to re-initialize after format!");
            printError("Program will exit. Please restart manually.");
            delete g_ofs;
            exit(1);
        }
    } else {
        printError("Format failed!");
        printError("File system may be corrupted. Please restart program.");
        delete g_ofs;
        exit(1);
    }
    
    pressEnter();
}

void changePermissions() {
    clearScreen();
    cout << "=== Change Permissions ===\n" << endl;
    
    string path = getInput("Enter file/directory path: ");
    string permStr = getInput("Enter permissions (e.g., 0644): ");
    
    uint32_t perms = stoul(permStr, nullptr, 8);
    
    int result = g_ofs->setPermissions(g_session, path.c_str(), perms);
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printSuccess("Permissions changed successfully");
    } else {
        printError("Failed: " + string(g_ofs->getErrorMessage(result)));
    }
    
    pressEnter();
}

bool login() {
    clearScreen();
    printBanner();
    
    cout << "=== Login ===" << endl;
    string username = getInput("Username: ");
    string password = getInput("Password: ");
    
    int result = g_ofs->userLogin(&g_session, username.c_str(), password.c_str());
    
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        g_current_user = username;
        
        SessionInfo info;
        g_ofs->getSessionInfo(g_session, &info);
        g_current_role = info.user.role;
        
        printSuccess("Welcome, " + username + "!");
        return true;
    } else {
        printError("Login failed: " + string(g_ofs->getErrorMessage(result)));
        return false;
    }
}

void logout() {
    g_ofs->userLogout(g_session);
    g_session = nullptr;
    g_current_user = "";
}

int main() {
    const char* omni_path = "BSCS24115.omni";
    const char* config_path = "compiled/default.uconf";
    
    g_ofs = new OFSCore();
    
    // Check if file exists
    ifstream check(omni_path);
    bool exists = check.good();
    check.close();
    
    if (!exists) {
        clearScreen();
        printBanner();
        printWarning("First time setup detected!");
        cout << "Creating new file system...\n" << endl;
        
        int result = g_ofs->format(omni_path, config_path);
        if (result != static_cast<int>(OFSErrorCodes::SUCCESS)) {
            printError("Failed to format file system!");
            delete g_ofs;
            return 1;
        }
        printSuccess("File system formatted successfully!");
        cout << "\nDefault Admin Account Created:" << endl;
        cout << "  Username: admin" << endl;
        cout << "  Password: admin123" << endl;
        cout << "\n[WARNING] Please change the admin password after first login!" << endl;
        pressEnter();
    }
    
    // Initialize
    int result = g_ofs->initialize(omni_path, config_path);
    if (result != static_cast<int>(OFSErrorCodes::SUCCESS)) {
        printError("Failed to initialize file system!");
        delete g_ofs;
        return 1;
    }
    
    // Login loop
    while (!login()) {
        string retry = getInput("\nTry again? (yes/no): ");
        if (retry != "yes") {
            g_ofs->shutdown();
            delete g_ofs;
            return 0;
        }
    }
    
    pressEnter();
    
    // Main loop
    while (true) {
        clearScreen();
        printBanner();
        printMenu();
        
        int choice;
        cin >> choice;
        cin.ignore();
        
        switch (choice) {
            case 1: createFile(); break;
            case 2: readFile(); break;
            case 3: editFile(); break;
            case 4: deleteFile(); break;
            case 5: renameFile(); break;
            case 6: createDirectory(); break;
            case 7: listDirectory(); break;
            case 8: deleteDirectory(); break;
            case 9: showInfo(); break;
            case 10: showStatistics(); break;
            case 11: showSessionInfo(); break;
            case 12: createUser(); break;
            case 13: deleteUser(); break;
            case 14: listUsers(); break;
            case 15: 
                formatFileSystem();
                // After format, user needs to login again
                if (g_session == nullptr) {
                    while (!login()) {
                        string retry = getInput("\nTry again? (yes/no): ");
                        if (retry != "yes") {
                            g_ofs->shutdown();
                            delete g_ofs;
                            cout << "\nGoodbye!" << endl;
                            return 0;
                        }
                    }
                }
                break;
            case 16: changePermissions(); break;
            case 17:
                logout();
                if (!login()) {
                    g_ofs->shutdown();
                    delete g_ofs;
                    return 0;
                }
                break;
            case 0:
                logout();
                g_ofs->shutdown();
                delete g_ofs;
                cout << "\nGoodbye!" << endl;
                return 0;
            default:
                printError("Invalid choice!");
                pressEnter();
        }
    }
    
    return 0;
}