#ifndef SERVER_H
#define SERVER_H

#include<arpa/inet.h>

#define BUFFER_SIZE 1024

// Estructura para un mensaje
typedef struct {
    char sender[50];
    char text[BUFFER_SIZE];
    unsigned int id;
    int has_attachment;    // 0 = SEND normal, 1= SENDATTACH
    char fileName[256];    // Nombre fichero adjunto
} Message;

// Estructura para un usuario
typedef struct {
    char username[50];
    char ip[INET_ADDRSTRLEN];
    int port;
    int connected;              // 0 desconectado, 1 conectado
    Message pending[100];
    int num_pending;
    unsigned int last_msg_id;   // contador por user
} User;

// Función que lee cadenas terminadas en \0
void recv_string(int sock, char *buffer);

// Funciones de gestión de usuarios
int user_exists(char *username);
int find_user(char *username);
int remove_user(char *username);
int add_user(char *username);

// Función para atender a un cliente
void *handle_client(void *arg);

#endif // SERVER_H