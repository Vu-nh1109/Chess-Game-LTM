#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef enum {
    MSG_GAME_BOARD,
    MSG_GET_BOARD,
    MSG_MOVE,
    MSG_REQUEST_DRAW,
    MSG_ACCEPT_DRAW,
    MSG_REFUSE_DRAW,
    MSG_SURRENDER,
    MSG_LOGIN,
    MSG_SIGNUP,
    MSG_LOGOUT,
    MSG_READY,
    MSG_CHAT,
    MSG_ERROR,
    MSG_SUCCESS,
    MSG_GAME_WIN,
    MSG_GAME_LOSE,
    MSG_GAME_DRAW,
    MSG_WAIT,
    MSG_GET_LEADERBOARD,
    MSG_LEADERBOARD,
    MSG_GET_HISTORY,
    MSG_HISTORY
} MessageType;

typedef enum {
    STATUS_ERROR,
    STATUS_SUCCESS
} MessageStatus;

typedef struct {
    MessageType type;
    MessageStatus status;
    int data_length;
    char data[1024];
} Message;

// Move specific data
typedef struct {
    int src_row;
    int src_col;
    int dest_row;
    int dest_col;
} MoveData;

// Auth specific data
typedef struct {
    char username[50];
    char password[50];
} AuthData;

// Chat specific data
typedef struct {
    char message[512];
} ChatData;

Message create_message(MessageType type, void* data, int length);
void send_message(int sock, Message* msg);
Message receive_message(int sock);
void free_message_data(Message* msg);

#endif