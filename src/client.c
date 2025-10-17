#include "../include/common.h"
#include <SDL.h>
#include <SDL_ttf.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define INPUT_HEIGHT 40
#define MAX_MESSAGES 100
#define MAX_INPUT_LENGTH 256

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    int running;
    int socket;
    char username[USERNAME_SIZE];
    char messages[MAX_MESSAGES][BUFFER_SIZE];
    int message_count;
    char input_text[MAX_INPUT_LENGTH];
    int input_length;
    pthread_t recv_thread;
} ChatClient;

ChatClient client;

void add_message(const char *message) {
    if (client.message_count >= MAX_MESSAGES) {
        // Shift messages up
        for (int i = 0; i < MAX_MESSAGES - 1; i++) {
            strcpy(client.messages[i], client.messages[i + 1]);
        }
        client.message_count = MAX_MESSAGES - 1;
    }
    strncpy(client.messages[client.message_count], message, BUFFER_SIZE - 1);
    client.message_count++;
}

void render_text(const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Blended(client.font, text, color);
    if (!surface) return;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(client.renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(client.renderer, texture, NULL, &rect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void render_ui() {
    // Clear screen
    SDL_SetRenderDrawColor(client.renderer, 30, 30, 30, 255);
    SDL_RenderClear(client.renderer);

    // Draw input box
    SDL_Rect input_box = {10, WINDOW_HEIGHT - INPUT_HEIGHT - 10, WINDOW_WIDTH - 20, INPUT_HEIGHT};
    SDL_SetRenderDrawColor(client.renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(client.renderer, &input_box);
    SDL_SetRenderDrawColor(client.renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(client.renderer, &input_box);

    // Draw input text
    SDL_Color white = {255, 255, 255, 255};
    if (client.input_length > 0) {
        render_text(client.input_text, 15, WINDOW_HEIGHT - INPUT_HEIGHT - 5, white);
    } else {
        SDL_Color gray = {150, 150, 150, 255};
        render_text("Type your message here...", 15, WINDOW_HEIGHT - INPUT_HEIGHT - 5, gray);
    }

    // Draw messages
    int y = 10;
    int visible_messages = (WINDOW_HEIGHT - INPUT_HEIGHT - 40) / 25;
    int start_index = (client.message_count > visible_messages) ?
                      client.message_count - visible_messages : 0;

    for (int i = start_index; i < client.message_count; i++) {
        render_text(client.messages[i], 10, y, white);
        y += 25;
    }

    SDL_RenderPresent(client.renderer);
}

void *receive_messages(void *arg) {
    (void)arg; // Unused parameter
    Message msg;
    int bytes_received;

    while (client.running) {
        bytes_received = recv(client.socket, &msg, sizeof(Message), 0);

        if (bytes_received <= 0) {
            printf("[CLIENT] Connection to server lost\n");
            client.running = 0;
            break;
        }

        char display_msg[BUFFER_SIZE + USERNAME_SIZE + 10];

        switch (msg.type) {
            case MSG_JOIN:
                snprintf(display_msg, sizeof(display_msg), "*** %s joined the chat ***", msg.username);
                add_message(display_msg);
                break;

            case MSG_LEAVE:
                snprintf(display_msg, sizeof(display_msg), "*** %s left the chat ***", msg.username);
                add_message(display_msg);
                break;

            case MSG_TEXT:
                snprintf(display_msg, sizeof(display_msg), "%s: %s", msg.username, msg.content);
                add_message(display_msg);
                break;

            case MSG_USER_LIST:
                snprintf(display_msg, sizeof(display_msg), "Online users: %s", msg.content);
                add_message(display_msg);
                break;
        }
    }

    return NULL;
}

void send_message(const char *text) {
    Message msg;
    msg.type = MSG_TEXT;
    strncpy(msg.username, client.username, USERNAME_SIZE - 1);
    strncpy(msg.content, text, BUFFER_SIZE - 1);

    if (send(client.socket, &msg, sizeof(Message), 0) < 0) {
        add_message("Error: Failed to send message");
    } else {
        char display_msg[BUFFER_SIZE + USERNAME_SIZE + 10];
        snprintf(display_msg, sizeof(display_msg), "You: %s", text);
        add_message(display_msg);
    }
}

void handle_input(SDL_Event *event) {
    if (event->type == SDL_TEXTINPUT) {
        if (client.input_length < MAX_INPUT_LENGTH - 1) {
            strcat(client.input_text, event->text.text);
            client.input_length = strlen(client.input_text);
        }
    } else if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_BACKSPACE && client.input_length > 0) {
            client.input_text[client.input_length - 1] = '\0';
            client.input_length--;
        } else if (event->key.keysym.sym == SDLK_RETURN && client.input_length > 0) {
            send_message(client.input_text);
            memset(client.input_text, 0, MAX_INPUT_LENGTH);
            client.input_length = 0;
        }
    }
}

int connect_to_server(const char *server_ip) {
    struct sockaddr_in server_addr;

    client.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client.socket < 0) {
        printf("Error: Failed to create socket\n");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(client.socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Failed to connect to server\n");
        close(client.socket);
        return -1;
    }

    // Send join message
    Message join_msg;
    join_msg.type = MSG_JOIN;
    strncpy(join_msg.username, client.username, USERNAME_SIZE - 1);
    snprintf(join_msg.content, BUFFER_SIZE, "%s has joined the chat", client.username);

    if (send(client.socket, &join_msg, sizeof(Message), 0) < 0) {
        printf("Error: Failed to send join message\n");
        close(client.socket);
        return -1;
    }

    printf("[CLIENT] Connected to server at %s:%d\n", server_ip, PORT);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <username> <server_ip>\n", argv[0]);
        printf("Example: %s Alice 127.0.0.1\n", argv[0]);
        return 1;
    }

    // Initialize client
    memset(&client, 0, sizeof(ChatClient));
    strncpy(client.username, argv[1], USERNAME_SIZE - 1);
    client.running = 1;

    // Connect to server
    if (connect_to_server(argv[2]) < 0) {
        return 1;
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        close(client.socket);
        return 1;
    }

    if (TTF_Init() < 0) {
        printf("TTF initialization failed: %s\n", TTF_GetError());
        SDL_Quit();
        close(client.socket);
        return 1;
    }

    // Create window
    client.window = SDL_CreateWindow("Chatroom Client",
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED,
                                      WINDOW_WIDTH, WINDOW_HEIGHT,
                                      SDL_WINDOW_SHOWN);
    if (!client.window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        close(client.socket);
        return 1;
    }

    // Create renderer
    client.renderer = SDL_CreateRenderer(client.window, -1, SDL_RENDERER_ACCELERATED);
    if (!client.renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(client.window);
        TTF_Quit();
        SDL_Quit();
        close(client.socket);
        return 1;
    }

    // Load font (use system default font)
    client.font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 18);
    if (!client.font) {
        // Try alternative font path for other systems
        client.font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18);
        if (!client.font) {
            printf("Font loading failed: %s\n", TTF_GetError());
            printf("Please ensure you have fonts installed on your system\n");
            SDL_DestroyRenderer(client.renderer);
            SDL_DestroyWindow(client.window);
            TTF_Quit();
            SDL_Quit();
            close(client.socket);
            return 1;
        }
    }

    // Add welcome message
    char welcome_msg[BUFFER_SIZE];
    snprintf(welcome_msg, BUFFER_SIZE, "Welcome to the chatroom, %s!", client.username);
    add_message(welcome_msg);

    // Start receive thread
    if (pthread_create(&client.recv_thread, NULL, receive_messages, NULL) != 0) {
        printf("Failed to create receive thread\n");
        TTF_CloseFont(client.font);
        SDL_DestroyRenderer(client.renderer);
        SDL_DestroyWindow(client.window);
        TTF_Quit();
        SDL_Quit();
        close(client.socket);
        return 1;
    }

    // Enable text input
    SDL_StartTextInput();

    // Main event loop
    SDL_Event event;
    while (client.running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                client.running = 0;
            } else {
                handle_input(&event);
            }
        }

        render_ui();
        SDL_Delay(16); // ~60 FPS
    }

    // Cleanup
    SDL_StopTextInput();
    pthread_join(client.recv_thread, NULL);
    close(client.socket);
    TTF_CloseFont(client.font);
    SDL_DestroyRenderer(client.renderer);
    SDL_DestroyWindow(client.window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
