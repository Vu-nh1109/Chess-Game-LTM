#include "protocol.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#define MAX_DATA_SIZE 1024

Message create_message(MessageType type, void* data, int length) {
    Message msg;
    msg.type = type;
    msg.data_length = length;
    memcpy(msg.data, data, length);
    return msg;
}

void send_message(int sock, Message* msg) {
    send(sock, msg, sizeof(Message), 0);
}

Message receive_message(int sock) {
    Message msg;
    int bytes_received = recv(sock, &msg, sizeof(Message), 0);
    if (bytes_received <= 0) {
        msg.status = STATUS_ERROR;
        msg.data_length = 0;
    } else {
        msg.status = STATUS_SUCCESS;
    }
    return msg;
}

void free_message_data(Message* msg) {
    if (msg) {
        memset(msg->data, 0, MAX_DATA_SIZE);
    }
}