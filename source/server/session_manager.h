#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

struct ClientSession {
    int client_sock;
    void* session;
    ClientSession* next;
};

// Functions
void* get_session(int client_sock);
void set_session(int client_sock, void* session);
void remove_session(int client_sock);

#endif
