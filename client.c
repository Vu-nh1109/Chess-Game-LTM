#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8000
#define BUFFER_SIZE 1024

int sock;
char buffer[BUFFER_SIZE];

// Function to clear the terminal screen
void clearScreen()
{
    system("clear");
}

// Function to handle receiving messages from the server
void *receiveMessages(void *arg)
{
    while (1)
    {
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
    }
    return NULL;
}

// Function to handle sending messages to the server
void *sendMessages(void *arg)
{
    while (1)
    {
        fgets(buffer, sizeof(buffer), stdin);

        // Remove trailing newline character
        buffer[strcspn(buffer, "\n")] = '\0';

        // Validate command format
        if (strncmp(buffer, "move ", 5) == 0)
        {
            int src_row, src_col, dest_row, dest_col;
            if (sscanf(buffer + 5, "%d %d %d %d", &src_row, &src_col, &dest_row, &dest_col) != 4)
            {
                printf("Invalid move format! Use: move src_row src_col dest_row dest_col\n");
                continue;
            }
        }
        else if (strcmp(buffer, "draw") == 0)
        {
            printf("Requesting a draw from the server...\n");
        }
        else if (strcmp(buffer, "surrender") == 0)
        {
            printf("Surrendering the game...\n");
        }
        else
        {
            printf("Invalid command! Valid commands: move, draw, surrender\n");
            continue;
        }

        // Send the command to the server
        send(sock, buffer, strlen(buffer), 0);
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
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
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

    close(sock);
    return 0;
}
