#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "protocol.h"

#define PORT 8000
#define BUFFER_SIZE 1024
#define CLEAR_LINE "\r\033[K"

int sock;
int board[8][8];
volatile int running = 1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
const char *PIECE_SYMBOLS[] = {
    "  ", "♙ ", "♘ ", "♗ ", "♖ ", "♕ ", "♔ ",
    "♟ ", "♞ ", "♝ ", "♜ ", "♛ ", "♚ "};
*/

void cleanup()
{
    running = 0;
    close(sock);
    pthread_mutex_destroy(&mutex);
}

// Function to clear the terminal screen
void clearScreen()
{
    system("clear");
}

// Function to handle receiving messages from the server
void *receiveMessages(void *arg)
{
    while (running)
    {
        /*
        int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0)
        {
            printf("Server disconnected.\n");
            break;
        }
        buffer[bytes_received] = '\0'; // Null-terminate the string

        // If the message contains "Current Board", clear the terminal
        if (strstr(buffer, "Current Board") != NULL)
        {
            clearScreen(); // Clear the terminal
        }

        // Print the received message
        printf("%s \n", buffer);
        */
        pthread_mutex_lock(&message_mutex);
        Message msg = receive_message(sock);
        if (msg.status == STATUS_ERROR)
        {
            printf("Server disconnected.\n");
            pthread_mutex_lock(&mutex);
            running = 0;
            pthread_mutex_unlock(&mutex);
            exit(1);
        }

        printf("%s", CLEAR_LINE);

        switch (msg.type)
        {
        case MSG_GAME_BOARD:
        {
            if (msg.data != NULL)
            {
                /*
                memcpy(board, msg.data, sizeof(board));
                clearScreen();
                for (int i = 0; i <= 8; i++)
                {
                    if (i > 0)
                        printf("%d ", i - 1);
                    else
                        printf("  ");
                    for (int j = 1; j <= 8; j++)
                    {
                        if (i == 0)
                            printf("%d ", j - 1);
                        else
                            printf("%s", PIECE_SYMBOLS[board[i - 1][j - 1]]);
                    }
                    printf("\n");
                }*/
                printf("=======================================\n");
                printf("%s\n", (char *)msg.data);
            }
            else
            {
                printf("Error: Invalid board data received.\n");
            }
            break;
        }
        case MSG_ERROR:
        {
            printf("Error: %s\n", (char *)msg.data);
            break;
        }
        case MSG_SUCCESS:
        {
            printf("Success: %s\n", (char *)msg.data);
            break;
        }
        case MSG_GAME_WIN:
        {
            printf("%s\n", (char *)msg.data);
            break;
        }
        case MSG_GAME_LOSE:
        {
            printf("%s\n", (char *)msg.data);
            break;
        }
        case MSG_GAME_DRAW:
        {
            printf("%s\n", (char *)msg.data);
            break;
        }
        case MSG_WAIT:
        {
            printf("Waiting for the second player to connect...\n");
            break;
        }
        case MSG_CHAT:
        {
            ChatData *chatData = (ChatData *)msg.data;
            printf("Opponent %s\n", chatData->message);
            break;
        }
        case MSG_LEADERBOARD:
        {
            printf("=======================================\n%s\n", (char *)msg.data);
            break;
        }
        case MSG_REQUEST_DRAW:
        {
            printf("Opponent requested a draw. Type 'accept draw' or 'refuse draw'.\n");
            break;
        }
        case MSG_REFUSE_DRAW:
        {
            printf("Opponent refused the draw.\n");
            break;
        }
        case MSG_HISTORY:
        {
            printf("=======================================\n%s\n", (char *)msg.data);
            break;
        }
        default:
            printf("Unknown message type received.\n");
            break;
        }
        fflush(stdout);
        free_message_data(&msg);
        pthread_mutex_unlock(&message_mutex);
    }
    return NULL;
}

// Function to handle sending messages to the server
void *sendMessages(void *arg)
{
    printf("Type 'help' for a list of commands.\n");
    fflush(stdout);
    while (running)
    {
        char buffer[BUFFER_SIZE];
        fgets(buffer, sizeof(buffer), stdin);

        Message msg;
        free_message_data(&msg);

        // Clear the current line
        printf("%s", CLEAR_LINE);

        // Remove trailing newline character
        buffer[strcspn(buffer, "\n")] = '\0';

        // Validate command format
        if (strncmp(buffer, "move ", 5) == 0)
        {
            MoveData moveData;
            sscanf(buffer + 5, "%d %d %d %d", &moveData.src_row, &moveData.src_col, &moveData.dest_row, &moveData.dest_col);
            Message msg = create_message(MSG_MOVE, &moveData, sizeof(MoveData));
            send_message(sock, &msg);

            /*
            int src_row, src_col, dest_row, dest_col;
            if (sscanf(buffer + 5, "%d %d %d %d", &src_row, &src_col, &dest_row, &dest_col) != 4)
            {
                printf("Invalid move format! Use: move src_row src_col dest_row dest_col\n");
                continue;
            }
            */
        }
        else if (strcmp(buffer, "request draw") == 0)
        {
            printf("Requesting a draw from the server...\n");
            Message msg = create_message(MSG_REQUEST_DRAW, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "accept draw") == 0)
        {
            printf("Accepting the draw...\n");
            Message msg = create_message(MSG_ACCEPT_DRAW, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "refuse draw") == 0)
        {
            printf("Refusing the draw...\n");
            Message msg = create_message(MSG_REFUSE_DRAW, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "surrender") == 0)
        {
            printf("Surrendering the game...\n");
            Message msg = create_message(MSG_SURRENDER, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "exit") == 0)
        {
            printf("Exiting the game...\n");
            pthread_mutex_lock(&mutex);
            running = 0;
            pthread_mutex_unlock(&mutex);
            exit(0);
        }
        else if (strncmp(buffer, "login ", 6) == 0)
        {
            char username[50], password[50];
            if (sscanf(buffer + 6, "%s %s", username, password) == 2)
            {
                AuthData loginData = {0};
                strncpy(loginData.username, username, sizeof(loginData.username) - 1);
                strncpy(loginData.password, password, sizeof(loginData.password) - 1);
                Message msg = create_message(MSG_LOGIN, &loginData, sizeof(AuthData));
                send_message(sock, &msg);
            }
            else
            {
                printf("Invalid login format! Use: login username password\n");
            }
        }
        else if (strncmp(buffer, "signup ", 7) == 0)
        {
            char username[50], password[50];
            if (sscanf(buffer + 7, "%s %s", username, password) == 2)
            {
                AuthData signupData = {0};
                strncpy(signupData.username, username, sizeof(signupData.username) - 1);
                strncpy(signupData.password, password, sizeof(signupData.password) - 1);
                Message msg = create_message(MSG_SIGNUP, &signupData, sizeof(AuthData));
                send_message(sock, &msg);
            }
            else
            {
                printf("Invalid signup format! Use: signup username password\n");
            }
        }
        else if (strcmp(buffer, "logout") == 0)
        {
            Message msg = create_message(MSG_LOGOUT, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "ready") == 0)
        {
            Message msg = create_message(MSG_READY, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strncmp(buffer, "chat ", 5) == 0)
        {
            ChatData chatData = {0};
            strncpy(chatData.message, buffer + 5, sizeof(chatData.message) - 1);
            Message msg = create_message(MSG_CHAT, &chatData, sizeof(ChatData));
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "leaderboard") == 0)
        {
            Message msg = create_message(MSG_GET_LEADERBOARD, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "clear") == 0)
        {
            clearScreen();
        }
        else if (strcmp(buffer, "current board") == 0)
        {
            Message msg = create_message(MSG_GET_BOARD, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "history") == 0)
        {
            Message msg = create_message(MSG_GET_HISTORY, NULL, 0);
            send_message(sock, &msg);
        }
        else if (strcmp(buffer, "help") == 0)
        {
            printf("List of commands:\n");
            printf("current board - Get the current board state\n");
            printf("move src_row src_col dest_row dest_col - Move a piece on the board\n");
            printf("request draw - Request a draw from the opponent\n");
            printf("surrender - Surrender the game\n");
            printf("exit - Exit the game\n");
            printf("login <username> <password> - Login to the server\n");
            printf("signup <username> <password> - Signup for a new account\n");
            printf("logout - Logout from the server\n");
            printf("ready - Ready up for a new game\n");
            printf("chat <message> - Send a chat message to the opponent\n");
            printf("leaderboard - Get the current leaderboard\n");
            printf("clear - Clear the terminal screen\n");
        }
        else
        {
            printf("Invalid command! Check \"help\"\n");
            continue;
        }

        free_message_data(&msg);
        memset(buffer, 0, sizeof(buffer));
    }
    return NULL;
}

int main()
{
    struct sockaddr_in server;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("172.31.73.220");
    server.sin_port = htons(PORT);

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Connection failed");
        return -1;
    }

    // Create threads for receiving and sending messages
    pthread_t recvThread, sendThread;
    pthread_create(&recvThread, NULL, receiveMessages, NULL);
    pthread_create(&sendThread, NULL, sendMessages, NULL);

    // Wait for threads to finish
    pthread_join(recvThread, NULL);
    pthread_join(sendThread, NULL);

    cleanup();
    return 0;
}
