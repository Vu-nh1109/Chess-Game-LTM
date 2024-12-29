#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2

// Chess pieces representation
#define EMPTY 0
#define PAWN_G1 1
#define KNIGHT_G1 2
#define BISHOP_G1 3
#define ROOK_G1 4
#define QUEEN_G1 5
#define KING_G1 6
#define PAWN_G2 7
#define KNIGHT_G2 8
#define BISHOP_G2 9
#define ROOK_G2 10
#define QUEEN_G2 11
#define KING_G2 12

const char *PIECE_SYMBOLS[] = {
    "  ", "♙ ", "♘ ", "♗ ", "♖ ", "♕ ", "♔ ",
    "♟ ", "♞ ", "♝ ", "♜ ", "♛ ", "♚ "};

int board[8][8];              // Chessboard state
int currentPlayer = 1;        // Track whose turn it is
int connectedClients = 0;     // Track number of connected clients
int client_sock[MAX_CLIENTS]; // Array to hold client sockets
int draw_requested = 0;
int ganeState = 0;            // 0 if game hasn't started

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void initializeBoard()
{
    // Initialize the chessboard with pieces
    board[0][0] = ROOK_G1;
    board[0][1] = KNIGHT_G1;
    board[0][2] = BISHOP_G1;
    board[0][3] = KING_G1;
    board[0][4] = QUEEN_G1;
    board[0][5] = BISHOP_G1;
    board[0][6] = KNIGHT_G1;
    board[0][7] = ROOK_G1;
    for (int i = 0; i < 8; i++)
        board[1][i] = PAWN_G1;
    for (int i = 0; i < 8; i++)
        board[6][i] = PAWN_G2;
    board[7][0] = ROOK_G2;
    board[7][1] = KNIGHT_G2;
    board[7][2] = BISHOP_G2;
    board[7][3] = KING_G2;
    board[7][4] = QUEEN_G2;
    board[7][5] = BISHOP_G2;
    board[7][6] = KNIGHT_G2;
    board[7][7] = ROOK_G2;
    for (int i = 2; i < 6; i++)
        for (int j = 0; j < 8; j++)
            board[i][j] = EMPTY;
}

void printBoardToAllClients()
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "Current Board:\n");
    for (int i = 0; i < 9; i++)
    {
        if (i > 0)
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%d ", i - 1);
        else
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "  ");
        for (int j = 1; j < 9; j++)
        {
            if (i == 0)
                snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%d ", j - 1);
            else
                snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%s", PIECE_SYMBOLS[board[i - 1][j - 1]]);
        }
        snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\n");
    }
    const char *turn_msg = (currentPlayer == 1) ? "Player 1's turn (White)" : "Player 2's turn (Black)";
    snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%s\n", turn_msg);
    snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "Enter your command (move src_row src_col dest_row dest_col, draw, surrender): \n");

    // Send the updated board to all connected clients
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sock[i] != -1)
        { // Check if client is connected
            if (send(client_sock[i], buffer, strlen(buffer), 0) == -1)
            {
                // Handle disconnection if sending fails
                printf("Client %d disconnected.\n", i);
                close(client_sock[i]);
                client_sock[i] = -1;
                connectedClients--; // Decrease the count of connected clients
            }
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

int checkForWin(int player)
{
    int kingPiece = (player == 1) ? KING_G2 : KING_G1; // Opponent's king

    // Check if the opponent's king is captured
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (board[i][j] == kingPiece)
            {
                return 0; // The king is still on the board, no winner yet
            }
        }
    }

    return 1; // Opponent's king is captured, this player wins
}

int isValidMove(int src_row, int src_col, int dest_row, int dest_col, int player)
{
    // Check if the source and destination are within bounds
    if (src_row < 0 || src_row >= 8 || src_col < 0 || src_col >= 8 ||
        dest_row < 0 || dest_row >= 8 || dest_col < 0 || dest_col >= 8)
    {
        return 0; // Out of bounds
    }

    int piece = board[src_row][src_col];
    int target = board[dest_row][dest_col];

    // Check if the source square has a piece
    if (piece == EMPTY)
        return 0;

    // Check if the piece belongs to the current player
    if ((player == 1 && piece > 6) || (player == 2 && piece <= 6))
        return 0;

    // Basic movement rules for each piece
    switch (piece)
    {
    case PAWN_G1:
        if (src_col == dest_col && dest_row == src_row + 1 && target == EMPTY)
            return 1; // Move forward
        if (src_col == dest_col && src_row == 1 && dest_row == 3 && target == EMPTY)
            return 1; // Double move
        if (abs(dest_col - src_col) == 1 && dest_row == src_row + 1 && target > 6)
            return 1; // Capture
        break;
    case PAWN_G2:
        if (src_col == dest_col && dest_row == src_row - 1 && target == EMPTY)
            return 1; // Move forward
        if (src_col == dest_col && src_row == 6 && dest_row == 4 && target == EMPTY)
            return 1; // Double move
        if (abs(dest_col - src_col) == 1 && dest_row == src_row - 1 && target <= 6)
            return 1; // Capture
        break;
    case KNIGHT_G1:
    case KNIGHT_G2:
        if ((abs(src_row - dest_row) == 2 && abs(src_col - dest_col) == 1) ||
            (abs(src_row - dest_row) == 1 && abs(src_col - dest_col) == 2))
        {
            return target == EMPTY || (player == 1 && target > 6) || (player == 2 && target <= 6); // Capture or move
        }
        break;
    case BISHOP_G1:
    case BISHOP_G2:
        if (abs(src_row - dest_row) == abs(src_col - dest_col) && (target == EMPTY || (player == 1 && target > 6) || (player == 2 && target <= 6)))
        {
            // Check for obstacles
            int row_step = (dest_row - src_row) / abs(dest_row - src_row);
            int col_step = (dest_col - src_col) / abs(dest_col - src_col);
            for (int i = 1; i < abs(dest_row - src_row); i++)
            {
                if (board[src_row + i * row_step][src_col + i * col_step] != EMPTY)
                    return 0; // Obstacle
            }
            return 1; // Valid move
        }
        break;
    case ROOK_G1:
    case ROOK_G2:
        if ((src_row == dest_row || src_col == dest_col) && (target == EMPTY || (player == 1 && target > 6) || (player == 2 && target <= 6)))
        {
            // Check for obstacles
            int step = (src_row == dest_row) ? (dest_col - src_col) / abs(dest_col - src_col) : (dest_row - src_row) / abs(dest_row - src_row);
            for (int i = 1; i < (src_row == dest_row ? abs(dest_col - src_col) : abs(dest_row - src_row)); i++)
            {
                if (board[src_row + (src_row == dest_row ? 0 : i * step)][src_col + (src_col == dest_col ? 0 : i * step)] != EMPTY)
                    return 0; // Obstacle
            }
            return 1; // Valid move
        }
        break;
    case QUEEN_G1:
    case QUEEN_G2:
        if ((abs(src_row - dest_row) == abs(src_col - dest_col) || src_row == dest_row || src_col == dest_col) && (target == EMPTY || (player == 1 && target > 6) || (player == 2 && target <= 6)))
        {
            // Check for obstacles
            int row_step = (dest_row - src_row) ? (dest_row - src_row) / abs(dest_row - src_row) : 0;
            int col_step = (dest_col - src_col) ? (dest_col - src_col) / abs(dest_col - src_col) : 0;
            for (int i = 1; i < (src_row == dest_row ? abs(dest_col - src_col) : abs(dest_row - src_row)); i++)
            {
                if (board[src_row + i * row_step][src_col + i * col_step] != EMPTY)
                    return 0; // Obstacle
            }
            return 1; // Valid move
        }
        break;
    case KING_G1:
    case KING_G2:
        if (abs(src_row - dest_row) <= 1 && abs(src_col - dest_col) <= 1)
        {
            return target == EMPTY || (player == 1 && target > 6) || (player == 2 && target <= 6); // Capture or move
        }
        break;
    }
    return 0; // Invalid move
}

void *handleClient(void *client_socket)
{
    int client_sock_local = *(int *)client_socket; // Renamed to avoid conflict with global client_sock
    pthread_mutex_lock(&client_mutex);
    connectedClients++;
    if (connectedClients == 1)
    {
        char wait_msg[] = "You are the first player. Waiting for another player...\n";
        send(client_sock_local, wait_msg, strlen(wait_msg), 0);
    }
    pthread_mutex_unlock(&client_mutex);

    // Wait for the second player to connect
    while (connectedClients < 2)
    {
        sleep(1); // Waiting for the second client
    }

    // Start the game
    if (connectedClients == 2)
    {
        char start_msg[] = "Both players are connected. Starting the game!\n";
        send(client_sock_local, start_msg, strlen(start_msg), 0);
        initializeBoard();
        printBoardToAllClients(); // Send the board to both clients

        char buffer[BUFFER_SIZE];
        while (1)
        {
            int bytes_received = recv(client_sock_local, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0)
            {
                printf("Client disconnected.\n");
                break;
            }
            buffer[bytes_received] = '\0'; // Null-terminate the string

            if (strncmp(buffer, "move ", 5) == 0)
            {
                if (currentPlayer != (client_sock_local == client_sock[0] ? 1 : 2))
                {
                    send(client_sock_local, "It's not your turn!\n", strlen("It's not your turn!\n"), 0);
                    continue; // Skip this iteration and wait for the correct turn
                }

                int src_row, src_col, dest_row, dest_col;
                sscanf(buffer + 5, "%d %d %d %d", &src_row, &src_col, &dest_row, &dest_col);

                if (isValidMove(src_row, src_col, dest_row, dest_col, currentPlayer))
                {
                    board[dest_row][dest_col] = board[src_row][src_col];
                    board[src_row][src_col] = EMPTY;
                    /*
                    if (checkForWin(currentPlayer == 1 ? 2 : 1))
                    {
                        char win_msg[] = "You win! The opponent's king has been captured.\n";
                        send(client_sock_local, win_msg, strlen(win_msg), 0);

                        // Notify the opponent about the loss
                        int opponent = (client_sock_local == client_sock[0]) ? client_sock[1] : client_sock[0];
                        send(opponent, "You lose! Your king has been captured.\n", strlen("You lose! Your king has been captured.\n"), 0);
                    }
                    */
                    currentPlayer = (currentPlayer == 1) ? 2 : 1;
                    printBoardToAllClients();
                }
                else
                {
                    send(client_sock_local, "Invalid move!\n", strlen("Invalid move!\n"), 0);
                }
            }
            else if (strcmp(buffer, "draw") == 0)
            {
                if (draw_requested == 0)
                {
                    // First player requests a draw
                    draw_requested = 1; // Set draw request
                    send(client_sock_local, "Draw request sent to opponent.\n", strlen("Draw request sent to opponent.\n"), 0);

                    // Notify opponent
                    int opponent = (client_sock_local == client_sock[0]) ? client_sock[1] : client_sock[0];
                    send(opponent, "Opponent requests a draw. Type 'draw' to accept.\n", strlen("Opponent requests a draw. Type 'draw' to accept.\n"), 0);
                }
                else
                {
                    // Second player accepts the draw
                    send(client_sock_local, "You accepted the draw. Game ends in a draw.\n", strlen("You accepted the draw. Game ends in a draw.\n"), 0);

                    // Notify opponent
                    int opponent = (client_sock_local == client_sock[0]) ? client_sock[1] : client_sock[0];
                    send(opponent, "Opponent accepted the draw. Game ends in a draw.\n", strlen("Opponent accepted the draw. Game ends in a draw.\n"), 0);

                    // End the game (implement proper cleanup here)
                    draw_requested = 0; // Reset draw state
                }
            }
            else if (strcmp(buffer, "surrender") == 0)
            {
                send(client_sock_local, "You surrendered. Game over.\n", strlen("You surrendered. Game over.\n"), 0);
                int opponent = (client_sock_local == client_sock[0]) ? client_sock[1] : client_sock[0];
                send(opponent, "Opponent surrendered. You win!\n", strlen("Opponent surrendered. You win!\n"), 0);
            }
            else
            {
                send(client_sock_local, "Unknown command.\n", strlen("Unknown command.\n"), 0);
            }
        }
    }

    // Handle client disconnection properly
    close(client_sock_local);
    pthread_mutex_lock(&client_mutex);
    connectedClients--;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sock[i] == client_sock_local)
        {
            client_sock[i] = -1;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    return NULL;
}

int main()
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    pthread_t threads[MAX_CLIENTS];

    // Initialize client_sock array to -1
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_sock[i] = -1;
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for clients to connect...\n");

    // Accept clients and create a new thread for each client
    while (1)
    {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Find a slot for the new client
        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_sock[i] == -1)
            {
                client_sock[i] = new_socket;
                pthread_create(&threads[i], NULL, handleClient, &client_sock[i]);
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);
    }

    return 0;
}