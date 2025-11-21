#include "session_manager.h"

ClientSession* session_list_head = nullptr;

void* get_session(int client_sock) {
    ClientSession* curr = session_list_head;
    while (curr) {
        if (curr->client_sock == client_sock) return curr->session;
        curr = curr->next;
    }
    return nullptr;
}

void set_session(int client_sock, void* session) {
    ClientSession* curr = session_list_head;
    while (curr) {
        if (curr->client_sock == client_sock) {
            curr->session = session;
            return;
        }
        curr = curr->next;
    }
    ClientSession* new_node = new ClientSession{client_sock, session, session_list_head};
    session_list_head = new_node;
}

void remove_session(int client_sock) {
    ClientSession *curr = session_list_head, *prev = nullptr;
    while (curr) {
        if (curr->client_sock == client_sock) {
            if (prev) prev->next = curr->next;
            else session_list_head = curr->next;
            delete curr;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}
