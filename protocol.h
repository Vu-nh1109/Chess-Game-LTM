#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef enum {
    MSG_GAME_BOARD,     //server sends board to client
    MSG_GET_BOARD,      //client requests board from server
    MSG_MOVE,           //client sends move data to server
    MSG_REQUEST_DRAW,   //1 client sends draw request to another client
    MSG_ACCEPT_DRAW,    //client accepts draw request 
    MSG_REFUSE_DRAW,    //client refuses draw request
    MSG_SURRENDER,      //client sends request to end the game with a loss
    MSG_LOGIN,          //client requests loggin in
    MSG_SIGNUP,         //client requests sign up
    MSG_LOGOUT,         //client requests sign out
    MSG_READY,          //client sends signal to server to be ready for new game or play again
    MSG_CHAT,           //client sends chat message to the other client
    MSG_ERROR,          //server sends error message to client
    MSG_SUCCESS,        //server sends success message to client
    MSG_GAME_WIN,       //server sends win notification to winning client
    MSG_GAME_LOSE,      //server sends lose notification to losing client
    MSG_GAME_DRAW,      //server sends draw notification to both clients
    MSG_WAIT,           //server sends wait message to a ready client if there is no other connected client
    MSG_GET_LEADERBOARD,//client requests leaderboard info from server
    MSG_LEADERBOARD     //server sends leaderboard info to client
} MessageType;

typedef enum {
    STATUS_ERROR,       //returns error if can't connect to client or server
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