#include "../include/common.h"

typedef struct {
    int socket;
    char username[USERNAME_SIZE];
    int active;
} Client;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].active = 0;
        memset(clients[i].username, 0, USERNAME_SIZE);
    }
}

int add_client(int socket, const char *username) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].socket = socket;
            clients[i].active = 1;
            strncpy(clients[i].username, username, USERNAME_SIZE - 1);
            pthread_mutex_unlock(&clients_mutex);
            printf("[SERVER] Client '%s' connected (socket %d)\n", username, socket);
            return i;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1;
}

void remove_client(int index) {
    pthread_mutex_lock(&clients_mutex);
    if (index >= 0 && index < MAX_CLIENTS) {
        printf("[SERVER] Client '%s' disconnected (socket %d)\n",
               clients[index].username, clients[index].socket);
        close(clients[index].socket);
        clients[index].socket = -1;
        clients[index].active = 0;
        memset(clients[index].username, 0, USERNAME_SIZE);
    }
    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_message(Message *msg, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket != sender_socket) {
            send(clients[i].socket, msg, sizeof(Message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_user_list(int socket) {
    Message msg;
    msg.type = MSG_USER_LIST;
    char user_list[BUFFER_SIZE] = "";

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            strcat(user_list, clients[i].username);
            strcat(user_list, ",");
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    strncpy(msg.content, user_list, BUFFER_SIZE - 1);
    send(socket, &msg, sizeof(Message), 0);
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    Message msg;
    int bytes_received;
    int client_index = -1;

    // Receive initial join message
    bytes_received = recv(client_socket, &msg, sizeof(Message), 0);
    if (bytes_received <= 0 || msg.type != MSG_JOIN) {
        close(client_socket);
        return NULL;
    }

    // Add client to the list
    client_index = add_client(client_socket, msg.username);
    if (client_index == -1) {
        printf("[SERVER] Maximum clients reached. Rejecting connection.\n");
        close(client_socket);
        return NULL;
    }

    // Broadcast join message
    broadcast_message(&msg, -1);

    // Send user list to the new client
    send_user_list(client_socket);

    // Handle client messages
    while (1) {
        bytes_received = recv(client_socket, &msg, sizeof(Message), 0);

        if (bytes_received <= 0) {
            // Client disconnected
            Message leave_msg;
            leave_msg.type = MSG_LEAVE;
            strncpy(leave_msg.username, clients[client_index].username, USERNAME_SIZE - 1);
            snprintf(leave_msg.content, BUFFER_SIZE, "%s has left the chat",
                    clients[client_index].username);

            remove_client(client_index);
            broadcast_message(&leave_msg, -1);
            break;
        }

        // Broadcast the message to all other clients
        if (msg.type == MSG_TEXT) {
            printf("[CHAT] %s: %s\n", msg.username, msg.content);
            broadcast_message(&msg, client_socket);
        }
    }

    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    init_clients();

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Chatroom server started on port %d\n", PORT);
    printf("[SERVER] Waiting for connections...\n");

    // Accept client connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[SERVER] New connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Create thread to handle client
        int *client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_socket;

        if (pthread_create(&thread_id, NULL, handle_client, client_sock_ptr) != 0) {
            perror("Thread creation failed");
            close(client_socket);
            free(client_sock_ptr);
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
