#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include "protocol.h"

#define PORT 8000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2

typedef struct Account
{
    char username[50];
    char password[50];
    int wins;
    int losses;
    int draws;
    struct Account *next;
} Account;

Account *head = NULL;

void loadAccounts()
{
    FILE *file = fopen("account.txt", "r");
    if (file == NULL)
    {
        perror("Unable to open account file.");
        exit(1);
    }

    Account *current = NULL;
    while (1)
    {
        Account *newAccount = (Account *)malloc(sizeof(Account));
        if (fscanf(file, "%s %s %d %d %d", newAccount->username, newAccount->password, &newAccount->wins, &newAccount->losses, &newAccount->draws) == 5)
        {
            newAccount->next = NULL;

            if (head == NULL)
            {
                head = newAccount;
                current = head;
            }
            else
            {
                current->next = newAccount;
                current = current->next;
            }
        }
        else
        {
            free(newAccount);
            break;
        }
    }

    fclose(file);
}

void saveAccounts()
{
    FILE *file = fopen("account.txt", "w");
    if (file == NULL)
    {
        perror("Unable to save accounts.");
        exit(1);
    }

    Account *current = head;
    while (current != NULL)
    {
        fprintf(file, "%s %s %d %d %d\n", current->username, current->password, current->wins, current->losses, current->draws);
        current = current->next;
    }
    fclose(file);
}

Account *findAccount(char *username)
{
    Account *current = head;
    while (current != NULL)
    {
        if (strcmp(current->username, username) == 0)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void freeAccounts()
{
    Account *current = head;
    while (current != NULL)
    {
        Account *next = current->next;
        free(current);
        current = next;
    }
    head = NULL;
}

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

struct Player
{
    int client_sock;
    char username[50];
    bool ready;
} typedef Player;

int board[8][8];             // Chessboard state
int currentPlayer = 1;       // Track whose turn it is
int connectedClients = 0;    // Track number of connected clients
Player players[MAX_CLIENTS]; // Array to hold client sockets
int draw_requested = 0;      // 1 if a draw has been requested
int gameState = 0;           // 0 if game hasn't started

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

const char *PIECE_SYMBOLS[] = {
    "  ", "♙ ", "♘ ", "♗ ", "♖ ", "♕ ", "♔ ",
    "♟ ", "♞ ", "♝ ", "♜ ", "♛ ", "♚ "};

void sendBoardToBothClients()
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

        if (i == 4)
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%10s Player1: %s", "", players[0].username);
        else if (i == 5)
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%10s Player2: %s", "", players[1].username);
        snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\n");
    }
    const char *turn_msg = (currentPlayer == 1) ? "Player 1's turn (White)" : "Player 2's turn (Black)";
    snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%s\n", turn_msg);
    Message msg = create_message(MSG_GAME_BOARD, buffer, sizeof(buffer));
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (players[i].client_sock != -1)
        {
            send_message(players[i].client_sock, &msg);
        }
    }
    free_message_data(&msg);
}

/*
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
        if (players[i].client_sock != -1)
        { // Check if client is connected
            if (send(players[i].client_sock, buffer, strlen(buffer), 0) == -1)
            {
                // Handle disconnection if sending fails
                printf("Client %d disconnected.\n", i);
                close(players[i].client_sock);
                players[i].client_sock = -1;
                connectedClients--; // Decrease the count of connected clients
            }
        }
    }
    pthread_mutex_unlock(&client_mutex);
}
*/

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
        {
            if (target == KING_G2)
                return 2; // Checkmate
            return 1;     // Capture
        }
        break;
    case PAWN_G2:
        if (src_col == dest_col && dest_row == src_row - 1 && target == EMPTY)
            return 1; // Move forward
        if (src_col == dest_col && src_row == 6 && dest_row == 4 && target == EMPTY)
            return 1; // Double move
        if (abs(dest_col - src_col) == 1 && dest_row == src_row - 1 && target <= 6)
        {
            if (target == KING_G1)
                return 2; // Checkmate
            return 1;     // Capture
        }
        break;
    case KNIGHT_G1:
    case KNIGHT_G2:
        if ((abs(src_row - dest_row) == 2 && abs(src_col - dest_col) == 1) ||
            (abs(src_row - dest_row) == 1 && abs(src_col - dest_col) == 2))
        {
            if (target == EMPTY || (player == 1 && target > 6) || (player == 2 && target <= 6))
            {
                if ((player == 1 && target == KING_G2) || (player == 2 && target == KING_G1))
                    return 2; // Checkmate
                return 1;     // Capture or move
            }
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
            if ((player == 1 && target == KING_G2) || (player == 2 && target == KING_G1))
                return 2; // Checkmate
            return 1;     // Valid move
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
            if ((player == 1 && target == KING_G2) || (player == 2 && target == KING_G1))
                return 2; // Checkmate
            return 1;     // Valid move
        }
        break;
    case QUEEN_G1:
    case QUEEN_G2:
        // Check if move is like a rook (horizontal/vertical)
        if (src_row == dest_row || src_col == dest_col)
        {
            int step = (src_row == dest_row) ? (dest_col - src_col) / abs(dest_col - src_col) : (dest_row - src_row) / abs(dest_row - src_row);
            for (int i = 1; i < (src_row == dest_row ? abs(dest_col - src_col) : abs(dest_row - src_row)); i++)
            {
                if (board[src_row + (src_row == dest_row ? 0 : i * step)][src_col + (src_col == dest_col ? 0 : i * step)] != EMPTY)
                    return 0; // Obstacle
            }
            if (target == EMPTY || (player == 1 && target > 6) || (player == 2 && target <= 6))
            {
                if ((player == 1 && target == KING_G2) || (player == 2 && target == KING_G1))
                    return 2; // Checkmate
                return 1;     // Valid move
            }
        }
        // Check if move is like a bishop (diagonal)
        else if (abs(src_row - dest_row) == abs(src_col - dest_col))
        {
            int row_step = (dest_row - src_row) / abs(dest_row - src_row);
            int col_step = (dest_col - src_col) / abs(dest_col - src_col);
            for (int i = 1; i < abs(dest_row - src_row); i++)
            {
                if (board[src_row + i * row_step][src_col + i * col_step] != EMPTY)
                    return 0; // Obstacle
            }
            if (target == EMPTY || (player == 1 && target > 6) || (player == 2 && target <= 6))
            {
                if ((player == 1 && target == KING_G2) || (player == 2 && target == KING_G1))
                    return 2; // Checkmate
                return 1;     // Valid move
            }
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
    // bool waiting = false;

    pthread_mutex_lock(&client_mutex);
    connectedClients++;
    // if (connectedClients == 1)
    // {
    //     waiting = true;
    // }
    pthread_mutex_unlock(&client_mutex);

    // Wait for the second player to connect
    // while (waiting && connectedClients < 2)
    // {
    //     pthread_mutex_unlock(&client_mutex); // Ensure mutex is unlocked while sleeping
    //     sleep(1); // Waiting for the second client
    //     pthread_mutex_lock(&client_mutex);
    // }

    while (1)
    {

        /*
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(client_sock_local, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0)
        {
            printf("Client disconnected.\n");
            break;
        }
        buffer[bytes_received] = '\0'; // Null-terminate the string
        */

        // Handle client commands here
        Message msg, newMsg;
        char buffer[BUFFER_SIZE];
        msg = receive_message(client_sock_local);
        if (msg.status == STATUS_ERROR)
        {
            if (strlen(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username) > 0)
            {
                printf("Client %s disconnected.\n", players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
            }
            else
                printf("Client disconnected.\n");
            break;
        }
        switch (msg.type)
        {
        case MSG_MOVE:
        {
            if (gameState == 1)
            {
                MoveData *moveData = (MoveData *)msg.data;
                if ((currentPlayer == 1 && client_sock_local != players[0].client_sock) || (currentPlayer == 2 && client_sock_local != players[1].client_sock))
                {
                    send(client_sock_local, "It's not your turn!\n", strlen("It's not your turn!\n"), 0);
                    continue; // Skip this iteration and wait for the correct turn
                }

                int moveResult = isValidMove(moveData->src_row, moveData->src_col, moveData->dest_row, moveData->dest_col, currentPlayer);
                if (moveResult > 0)
                {
                    board[moveData->dest_row][moveData->dest_col] = board[moveData->src_row][moveData->src_col];
                    board[moveData->src_row][moveData->src_col] = EMPTY;
                    printf("Player %s moved from (%d, %d) to (%d, %d)\n", players[currentPlayer - 1].username, moveData->src_row, moveData->src_col, moveData->dest_row, moveData->dest_col);
                    if (moveResult == 2) // Checkmate
                    {
                        // printBoardToAllClients();
                        // Message newMsg = create_message(MSG_GAME_BOARD, board, sizeof(board));
                        // send_message(client_sock_local, &newMsg);
                        // send_message(opponent, &newMsg);
                        // sendBoardToBothClients();
                        int opponent = (client_sock_local == players[0].client_sock) ? players[1].client_sock : players[0].client_sock;
                        Message newMsg = create_message(MSG_GAME_WIN, "Checkmate! You win!\nType\"ready\" to play again", strlen("Checkmate! You win!\nType\"ready\" to play again"));
                        printf("Player %s wins!\n", players[currentPlayer - 1].username);
                        send_message(client_sock_local, &newMsg);
                        newMsg = create_message(MSG_GAME_LOSE, "Checkmate! You lose!Type\"ready\" to play again", strlen("Checkmate! You lose!Type\"ready\" to play again"));
                        send_message(opponent, &newMsg);
                        gameState = 0;
                        // Update win/loss records
                        Account *winner = findAccount(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
                        Account *loser = findAccount(players[(client_sock_local == players[0].client_sock) ? 1 : 0].username);
                        if (winner)
                            winner->wins++;
                        if (loser)
                            loser->losses++;
                        saveAccounts();
                    }
                    else
                    {
                        currentPlayer = (currentPlayer == 1) ? 2 : 1;
                        // printBoardToAllClients();
                        // Message newMsg = create_message(MSG_GAME_BOARD, board, sizeof(board));
                        // send_message(client_sock_local, &newMsg);
                        // int opponent = (client_sock_local == players[0].client_sock) ? players[1].client_sock : players[0].client_sock;
                        // send_message(opponent, &newMsg);
                        sendBoardToBothClients();
                    }
                }
                else
                {
                    Message newMsg = create_message(MSG_ERROR, "Invalid move!", strlen("Invalid move!"));
                    send_message(client_sock_local, &newMsg);
                }
            }
            else
            {
                Message newMsg = create_message(MSG_ERROR, "Game hasn't started yet.", strlen("Game hasn't started yet."));
                send_message(client_sock_local, &newMsg);
            }

            break;
        }
        case MSG_REQUEST_DRAW:
        {
            if (gameState == 0)
            {
                Message newMsg = create_message(MSG_ERROR, "Game hasn't started yet.", strlen("Game hasn't started yet."));
                send_message(client_sock_local, &newMsg);
            }
            else
            {
                if (draw_requested == 0)
                {
                    // First player requests a draw
                    printf("Player %s requests a draw.\n", players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
                    draw_requested = 1; // Set draw request
                    Message newMsg = create_message(MSG_SUCCESS, "Draw request sent to opponent.", strlen("Draw request sent to opponent."));
                    send_message(client_sock_local, &newMsg);

                    // Notify opponent
                    int opponent = (client_sock_local == players[0].client_sock) ? players[1].client_sock : players[0].client_sock;
                    newMsg = create_message(MSG_REQUEST_DRAW, NULL, 0);
                    send_message(opponent, &newMsg);
                }
                else
                {
                    Message newMsg = create_message(MSG_ERROR, "Draw already requested.", strlen("Draw already requested."));
                    send_message(client_sock_local, &newMsg);
                }
            }
            break;
        }
        case MSG_ACCEPT_DRAW:
        {
            if (gameState == 0)
            {
                Message newMsg = create_message(MSG_ERROR, "Game hasn't started yet.", strlen("Game hasn't started yet."));
                send_message(client_sock_local, &newMsg);
            }
            else
            {
                if (draw_requested == 1)
                {
                    printf("Player %s accepted the draw.\n", players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
                    gameState = 0;
                    draw_requested = 0;

                    Message newMsg = create_message(MSG_GAME_DRAW, NULL, 0);
                    send_message(client_sock_local, &newMsg);
                    int opponent = (client_sock_local == players[0].client_sock) ? players[1].client_sock : players[0].client_sock;
                    send_message(opponent, &newMsg);
                    // Update draw records for both players
                    Account *player1 = findAccount(players[0].username);
                    Account *player2 = findAccount(players[1].username);
                    if (player1)
                        player1->draws++;
                    if (player2)
                        player2->draws++;
                    saveAccounts();
                }
                else
                {
                    Message newMsg = create_message(MSG_ERROR, "No draw request to accept.", strlen("No draw request to accept."));
                    send_message(client_sock_local, &newMsg);
                }
            }
            break;
        }
        case MSG_REFUSE_DRAW:
        {
            if (gameState == 0)
            {
                Message newMsg = create_message(MSG_ERROR, "Game hasn't started yet.", strlen("Game hasn't started yet."));
                send_message(client_sock_local, &newMsg);
            }
            else
            {
                if (draw_requested == 1)
                {
                    printf("Player %s refused the draw.\n", players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
                    draw_requested = 0;
                    int opponent = (client_sock_local == players[0].client_sock) ? players[1].client_sock : players[0].client_sock;
                    Message newMsg = create_message(MSG_REFUSE_DRAW, "Opponent refused your request for a draw", strlen("Opponent refused your request for a draw"));
                    send_message(opponent, &newMsg);
                }
                else
                {
                    Message newMsg = create_message(MSG_ERROR, "No draw request to refuse.", strlen("No draw request to refuse."));
                    send_message(client_sock_local, &newMsg);
                }
            }
            break;
        }
        case MSG_SURRENDER:
        {
            if (gameState == 0)
            {
                Message newMsg = create_message(MSG_ERROR, "Game hasn't started yet.", strlen("Game hasn't started yet."));
                send_message(client_sock_local, &newMsg);
            }
            else
            {
                printf("Player %s surrendered.\n", players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
                gameState = 0;
                Message newMsg = create_message(MSG_GAME_LOSE, "You surrendered. Game over.", strlen("You surrendered. Game over."));
                send_message(client_sock_local, &newMsg);
                int opponent = (client_sock_local == players[0].client_sock) ? players[1].client_sock : players[0].client_sock;
                newMsg = create_message(MSG_GAME_WIN, "Opponent surrendered. You win!", strlen("Opponent surrendered. You win!"));
                send_message(opponent, &newMsg);
                // Update win/loss records
                Account *winner = findAccount(players[(client_sock_local == players[0].client_sock) ? 1 : 0].username);
                Account *loser = findAccount(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
                if (winner)
                    winner->wins++;
                if (loser)
                    loser->losses++;
                saveAccounts();
            }
            break;
        }
        case MSG_LOGIN:
        {
            if (strlen(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username) > 0)
            {
                Message newMsg = create_message(MSG_ERROR, "You are already logged in.", strlen("You are already logged in."));
                send_message(client_sock_local, &newMsg);
            }
            else if (findAccount(((AuthData *)msg.data)->username) != NULL &&
                     strcmp(players[(client_sock_local == players[0].client_sock) ? 1 : 0].username, ((AuthData *)msg.data)->username) == 0)
            {
                Message newMsg = create_message(MSG_ERROR, "This account is already logged in by your opponent.", strlen("This account is already logged in by your opponent."));
                send_message(client_sock_local, &newMsg);
            }
            else
            {
                AuthData *authData = (AuthData *)msg.data;
                Account *account = findAccount(authData->username);
                if (account != NULL && strcmp(account->password, authData->password) == 0)
                {
                    strcpy(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username, authData->username);
                    Message newMsg = create_message(MSG_SUCCESS, "Login successful.", strlen("Login successful."));
                    send_message(client_sock_local, &newMsg);
                    printf("Player %s logged in.\n", authData->username);
                }
                else
                {
                    Message newMsg = create_message(MSG_ERROR, "Invalid username or password.", strlen("Invalid username or password."));
                    send_message(client_sock_local, &newMsg);
                }
            }
            break;
        }
        case MSG_SIGNUP:
        {
            if (strlen(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username) > 0)
            {
                Message newMsg = create_message(MSG_ERROR, "You are already logged in.", strlen("You are already logged in."));
                send_message(client_sock_local, &newMsg);
                break;
            }
            AuthData *authData = (AuthData *)msg.data;
            Account *account = findAccount(authData->username);
            if (account == NULL)
            {
                Account *newAccount = (Account *)malloc(sizeof(Account));
                strcpy(newAccount->username, authData->username);
                strcpy(newAccount->password, authData->password);
                newAccount->wins = 0;
                newAccount->losses = 0;
                newAccount->draws = 0;
                newAccount->next = head;
                head = newAccount;
                saveAccounts();
                Message newMsg = create_message(MSG_SUCCESS, "Account created successfully.", strlen("Account created successfully."));
                send_message(client_sock_local, &newMsg);
                printf("Player %s signed up.\n", authData->username);
            }
            else
            {
                Message newMsg = create_message(MSG_ERROR, "Username already exists.", strlen("Username already exists."));
                send_message(client_sock_local, &newMsg);
            }
            break;
        }
        case MSG_LOGOUT:
        {
            if (strlen(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username) == 0)
            {
                Message newMsg = create_message(MSG_ERROR, "You are not logged in.", strlen("You are not logged in."));
                send_message(client_sock_local, &newMsg);
            }
            else
            {
                printf("Player %s logged out.\n", players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
                strcpy(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username, "");
                Message newMsg = create_message(MSG_SUCCESS, "Logout successful.", strlen("Logout successful."));
                send_message(client_sock_local, &newMsg);
            }
            break;
        }
        case MSG_READY:
        {
            if (gameState == 1)
            {
                Message newMsg = create_message(MSG_ERROR, "Game has already started.", strlen("Game has already started."));
                send_message(client_sock_local, &newMsg);
                break;
            }
            if (strlen(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username) == 0)
            {
                Message newMsg = create_message(MSG_ERROR, "You are not logged in.", strlen("You are not logged in."));
                send_message(client_sock_local, &newMsg);
                break;
            }
            players[(client_sock_local == players[0].client_sock) ? 0 : 1].ready = true;
            Message newMsg = create_message(MSG_SUCCESS, "You are ready.", strlen("You are ready."));
            send_message(client_sock_local, &newMsg);
            int playerIndex = (client_sock_local == players[0].client_sock) ? 0 : 1;
            printf("Player %d (%s) is ready.\n", playerIndex + 1, players[playerIndex].username);
            if (connectedClients < 2)
            {
                Message newMsg = create_message(MSG_WAIT, NULL, 0);
                send_message(client_sock_local, &newMsg);
                break;
            }
            if (players[0].ready && players[1].ready)
            {
                printf("Game started.\n");
                gameState = 1;
                initializeBoard();
                players[0].ready = false;
                players[1].ready = false;
                // Message newMsg = create_message(MSG_GAME_BOARD, board, sizeof(board));
                // for (int i = 0; i < MAX_CLIENTS; i++)
                // {
                //     send_message(players[i].client_sock, &newMsg);
                // }
                // printBoardToAllClients();
                sendBoardToBothClients();
            }
            break;
        }
        case MSG_CHAT:
        {
            ChatData *chatData = (ChatData *)msg.data;
            if (!chatData || strlen(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username) == 0)
            {
                Message errMsg = create_message(MSG_ERROR, "You must be logged in to chat", strlen("You must be logged in to chat"));
                send_message(client_sock_local, &errMsg);
            }
            else
            {
                char buffer[BUFFER_SIZE];
                printf("%s: %s\n", players[(client_sock_local == players[0].client_sock) ? 0 : 1].username, chatData->message);
                snprintf(buffer, sizeof(buffer), "%s: %s", players[(client_sock_local == players[0].client_sock) ? 0 : 1].username, chatData->message);
                Message newMsg = create_message(MSG_CHAT, buffer, strlen(buffer));
                int opponent = players[(client_sock_local == players[0].client_sock) ? 1 : 0].client_sock;
                if (opponent != -1)
                {
                    send_message(opponent, &newMsg);
                }
                else
                {
                    Message errMsg = create_message(MSG_ERROR, "Opponent is not connected", strlen("Opponent is not connected"));
                    send_message(client_sock_local, &errMsg);
                }
            }
            break;
        }
        case MSG_GET_BOARD:
        {
            if (gameState == 0)
            {
                Message newMsg = create_message(MSG_ERROR, "Game hasn't started yet.", strlen("Game hasn't started yet."));
                send_message(client_sock_local, &newMsg);
            }
            else
            {
                // Message newMsg = create_message(MSG_GAME_BOARD, board, sizeof(board));
                // send_message(client_sock_local, &newMsg);
                sendBoardToBothClients();
            }
            break;
        }
        case MSG_GET_LEADERBOARD:
        {
            char buffer[BUFFER_SIZE];
            // Create array of accounts and scores
            typedef struct
            {
                Account *account;
                float score;
            } AccountScore;

            int count = 0;
            Account *current = head;
            while (current != NULL)
            {
                count++;
                current = current->next;
            }

            AccountScore scores[count];
            current = head;
            int i = 0;
            while (current != NULL)
            {
                scores[i].account = current;
                scores[i].score = current->wins + (current->draws * 0.5) - (current->losses * 0.5);
                i++;
                current = current->next;
            }

            // Sort by score descending
            for (i = 0; i < count - 1; i++)
            {
                for (int j = 0; j < count - i - 1; j++)
                {
                    if (scores[j].score < scores[j + 1].score)
                    {
                        AccountScore temp = scores[j];
                        scores[j] = scores[j + 1];
                        scores[j + 1] = temp;
                    }
                }
            }

            // Format leaderboard message
            for (i = 0; i < count; i++)
            {
                snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
                         "%s: %d wins, %d losses, %d draws (Score: %.1f)\n",
                         scores[i].account->username,
                         scores[i].account->wins,
                         scores[i].account->losses,
                         scores[i].account->draws,
                         scores[i].score);
            }

            Message newMsg = create_message(MSG_LEADERBOARD, buffer, strlen(buffer));
            send_message(client_sock_local, &newMsg);
            break;
        }
        default:
            Message newMsg = create_message(MSG_ERROR, "Unknown command.", strlen("Unknown command."));
            send_message(client_sock_local, &newMsg);
            break;
        }
        free_message_data(&msg);
        free_message_data(&newMsg);
        memset(buffer, 0, sizeof(buffer));
        /*
        if (strncmp(buffer, "move ", 5) == 0)
        {
            if (currentPlayer != (client_sock_local == client_sock[0] ? 1 : 2))
            {
                send(client_sock_local, "It's not your turn!\n", strlen("It's not your turn!\n"), 0);
                continue; // Skip this iteration and wait for the correct turn
            }

            int src_row, src_col, dest_row, dest_col;
            sscanf(buffer + 5, "%d %d %d %d", &src_row, &src_col, &dest_row, &dest_col);

            if (isValidMove(src_row, src_col, dest_row, dest_col, currentPlayer) > 0)
            {
                board[dest_row][dest_col] = board[src_row][src_col];
                board[src_row][src_col] = EMPTY;
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
        */
    }

    // Handle client disconnection properly
    int opponent = (client_sock_local == players[0].client_sock) ? players[1].client_sock : players[0].client_sock;
    if (opponent != -1)
    {
        Message newMsg = create_message(MSG_ERROR, "Opponent disconnected", strlen("Opponent disconnected"));
        send_message(opponent, &newMsg);

        if (gameState == 1)
        {
            newMsg = create_message(MSG_GAME_WIN, "You win!", strlen("You win!"));
            send_message(opponent, &newMsg);
            gameState = 0;
            // Update win/loss records
            Account *winner = findAccount(players[(client_sock_local == players[0].client_sock) ? 1 : 0].username);
            Account *loser = findAccount(players[(client_sock_local == players[0].client_sock) ? 0 : 1].username);
            if (winner)
                winner->wins++;
            if (loser)
                loser->losses++;
            saveAccounts();
        }
    }
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (players[i].client_sock == client_sock_local)
        {
            players[i].client_sock = -1;
            players[i].ready = false;
            strcpy(players[i].username, "");
            connectedClients--;
            break;
        }
    }
    close(client_sock_local);
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

    loadAccounts();

    // Initialize client_sock array to -1
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        players[i].client_sock = -1;
        players[i].ready = false;
        strcpy(players[i].username, "");
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
            if (players[i].client_sock == -1)
            {
                players[i].client_sock = new_socket;
                pthread_create(&threads[i], NULL, handleClient, &players[i].client_sock);
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);
    }

    close(server_fd);
    freeAccounts();
    return 0;
}