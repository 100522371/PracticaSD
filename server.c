#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_USERS 100

// información del usuario
typedef struct {
    char username[50];
    char ip[INET_ADDRSTRLEN];
    int port;
    int connected; // 0 desconectado, 1 conectado

    Message pending[100];
    int num_pending;
    unsigned int last_msg_id; // contador por user
} User;

typedef struct {
    char sender[50];
    char text[BUFFER_SIZE];
    unsigned int id;
} Message;

// Lista global de usuarios
User users[MAX_USERS];
int num_users = 0;
unsigned int global_msg_id = 0;

// Mutex para evitar problemas entre hilos
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

// Función que lee cadenas terminadas en \0
void recv_string(int sock, char *buffer) {
    int i = 0;
    char c;

    while (1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) break;

        if (c == '\0') break;

        buffer[i++] = c;
    }

    buffer[i] = '\0';
}

// Comprobar si un usuario ya existe
int user_exists(char *username) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return 1;
        }
    }
    return 0;
}

// devolver índice del user o -1 si no existe
int find_user(char *username) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

// Eliminar un usuario del array
int remove_user(char *username) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {

            // Desplazar elementos hacia la izda
            for (int j = i; j < num_users - 1; j++) {
                users[j] = users[j + 1];
            }

            num_users--;
            return 1; // si eliminado correctamente
        }
    }
    return 0; // si no encontrado
}

// Atender a un cliente
void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char operation[BUFFER_SIZE];

    recv_string(client_sock, operation);

    if (strcmp(operation, "REGISTER") == 0) {

        char username[50];
        recv_string(client_sock, username);
        pthread_mutex_lock(&users_mutex);

        if (user_exists(username)) {
            // Usuario ya existe
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> REGISTER %s FAIL\n", username);

        } else if (num_users < MAX_USERS) {
            // usuario nuevo
            strcpy(users[num_users].username, username);
            users[num_users].connected = 0;
            users[num_users].port = 0;
            users[num_users].ip[0] = '\0';
            users[num_users].num_pending = 0;
            users[num_users].last_msg_id = 0;

            num_users++;
            char code = 0;
            send(client_sock, &code, 1, 0);
            printf("s> REGISTER %s OK\n", username);

        } else {
            // Error 
            char code = 2;
            send(client_sock, &code, 1, 0);
            printf("s> REGISTER %s FAIL\n", username);
        }

        pthread_mutex_unlock(&users_mutex);

    } else if (strcmp(operation, "UNREGISTER") == 0) {

        char username[50];
        recv_string(client_sock, username);
        pthread_mutex_lock(&users_mutex);

        if (remove_user(username)) {
            // Usuario eliminado correctamente
            char code = 0;
            send(client_sock, &code, 1, 0);
            printf("s> UNREGISTER %s OK\n", username);

        } else {
            // Usuario no existe
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> UNREGISTER %s FAIL\n", username);
        }

        pthread_mutex_unlock(&users_mutex);

    } else if (strcmp(operation, "CONNECT") == 0) {
        char username[50];
        char port_str[20];

        recv_string(client_sock, username);
        recv_string(client_sock, port_str);

        int port = atoi(port_str);
        pthread_mutex_lock(&users_mutex);
        int idx = find_user(username);

        if (idx == -1) {
            // Usuario no existe
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> CONNECT %s FAIL\n", username);

        } else if (users[idx].connected) {
            // Ya conectado
            char code = 2;
            send(client_sock, &code, 1, 0);
            printf("s> CONNECT %s FAIL\n", username);

        } else {
            // Conectar usuario, obtener IP de cliente
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            if (getpeername(client_sock, (struct sockaddr *)&addr, &len) == 0) {
                inet_ntop(AF_INET, &addr.sin_addr, users[idx].ip, INET_ADDRSTRLEN);
            } else {
                strcpy(users[idx].ip, "unknown");
            }

            users[idx].port = port;
            users[idx].connected = 1;

            char code = 0;
            send(client_sock, &code, 1, 0);
            printf("s> CONNECT %s OK\n", username);

            // Enviar mensajes pendientes
            User *u = &users[idx];

            for (int i = 0; i < u->num_pending; i++) {
                int sock_dest = socket(AF_INET, SOCK_STREAM, 0);

                struct sockaddr_in dest_addr;
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(u->port);
                inet_pton(AF_INET, u->ip, &dest_addr.sin_addr);

                if (connect(sock_dest, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == 0) {

                    send(sock_dest, "SEND MESSAGE\0", strlen("SEND MESSAGE") + 1, 0);
                    send(sock_dest, u->pending[i].sender, strlen(u->pending[i].sender) + 1, 0);

                    char id_str[20];
                    sprintf(id_str, "%u", u->pending[i].id);
                    send(sock_dest, id_str, strlen(id_str) + 1, 0);
                    send(sock_dest, u->pending[i].text, strlen(u->pending[i].text) + 1, 0);
                    printf("s> SEND MESSAGE %u FROM %s TO %s\n", u->pending[i].id, u->pending[i].sender, username);
                }

                close(sock_dest);
            }

            // Vaciar cola
            u->num_pending = 0;
        }

        pthread_mutex_unlock(&users_mutex);
    } else if (strcmp(operation, "DISCONNECT") == 0) {
        char username[50];
        recv_string(client_sock, username);
        pthread_mutex_lock(&users_mutex);
        int idx = find_user(username);

        if (idx == -1) {
            // Usuario no existe
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> DISCONNECT %s FAIL\n", username);

        } else if (!users[idx].connected) {
            // Usuario existe pero no conectado
            char code = 2;
            send(client_sock, &code, 1, 0);
            printf("s> DISCONNECT %s FAIL\n", username);

        } else {
            // Limpiar datos de conexión de usuario qeu estaba conectado
            users[idx].connected = 0;
            users[idx].port = 0;
            users[idx].ip[0] = '\0';

            char code = 0;
            send(client_sock, &code, 1, 0);
            printf("s> DISCONNECT %s OK\n", username);
        }

        pthread_mutex_unlock(&users_mutex);

    } else if (strcmp(operation, "SEND") == 0) {
        char sender[50];
        char receiver[50];
        char message[BUFFER_SIZE];

        recv_string(client_sock, sender);
        recv_string(client_sock, receiver);
        recv_string(client_sock, message);

        pthread_mutex_lock(&users_mutex);

        int sender_idx = find_user(sender);
        int receiver_idx = find_user(receiver);

        if (sender_idx == -1 || receiver_idx == -1) {
            // o emisor o remitente no existen
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> SEND FAIL\n");

        } else {
            // Contador por usuario
            users[sender_idx].last_msg_id++;
            unsigned int msg_id = users[sender_idx].last_msg_id;

            Message m;
            strcpy(m.sender, sender);
            strcpy(m.text, message);
            m.id = msg_id;

            User *recv_user = &users[receiver_idx];
            recv_user->pending[recv_user->num_pending++] = m;

            char code = 0;
            send(client_sock, &code, 1, 0);

            // Enviar ID como string terminado en \0
            char id_str[20];
            sprintf(id_str, "%u", global_msg_id);
            send(client_sock, id_str, strlen(id_str) + 1, 0);

            printf("s> SEND MESSAGE %u FROM %s TO %s\n", global_msg_id, sender, receiver);

            // Enviar mensaje a receptor si está conectado
            if (users[receiver_idx].connected) {

                int sock_dest = socket(AF_INET, SOCK_STREAM, 0);

                struct sockaddr_in dest_addr;
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(users[receiver_idx].port);
                inet_pton(AF_INET, users[receiver_idx].ip, &dest_addr.sin_addr);

                if (connect(sock_dest, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == 0) {

                    //enviar protocolo servidor-cliente
                    send(sock_dest, "SEND_MESSAGE\0", strlen("SEND_MESSAGE") + 1, 0);
                    send(sock_dest, sender, strlen(sender) + 1, 0);

                    char id_str2[20];
                    sprintf(id_str2, "%u", global_msg_id);
                    send(sock_dest, id_str2, strlen(id_str2) + 1, 0);
                    send(sock_dest, message, strlen(message) + 1, 0);
                }
                close(sock_dest);

                // Notificar al emisor si está conectado
                if (users[sender_idx].connected) {
                    int sock_sender = socket(AF_INET, SOCK_STREAM, 0);

                    struct sockaddr_in sender_addr;
                    sender_addr.sin_family = AF_INET;
                    sender_addr.sin_port = htons(users[sender_idx].port);
                    inet_pton(AF_INET, users[sender_idx].ip, &sender_addr.sin_addr);

                    if (connect(sock_sender, (struct sockaddr *)&sender_addr, sizeof(sender_addr)) == 0) {
                        send(sock_sender, "SEND_MESS_ACK\0", strlen("SEND_MESS_ACK") + 1, 0);

                        char id_str3[20];
                        sprintf(id_str3, "%u", global_msg_id);
                        send(sock_sender, id_str3, strlen(id_str3) + 1, 0);
                    }
                    close(sock_sender);
                }
            }
        }
        pthread_mutex_unlock(&users_mutex);

    } else if (strcmp(operation, "USERS") == 0) {
        char requester[50];
        recv_string(client_sock, requester);
        pthread_mutex_lock(&users_mutex);
        int idx = find_user(requester);

        if (idx == -1 || !users[idx].connected) {
            // Usuario no exist o no está conectado
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> USERS FAIL\n");

        } else {
            // Contamos usuarios conectados
            int count = 0;
            for (int i = 0; i < num_users; i++) {
                if (users[i].connected) {
                    count++;
                }
            }

            char code = 0;
            send(client_sock, &code, 1, 0);

            // Enviar número de usuarios como string
            char count_str[10];
            sprintf(count_str, "%d", count);
            send(client_sock, count_str, strlen(count_str) + 1, 0);

            // Enviar nombres
            for (int i = 0; i < num_users; i++) {
                if (users[i].connected) {
                    send(client_sock, users[i].username, strlen(users[i].username) + 1, 0);
                }
            }
            printf("s> USERS OK\n");
        }
        pthread_mutex_unlock(&users_mutex);

    } else {
        // si otra operación 
        printf("s> UNKNOWN OPERATION\n");
    }   

    close(client_sock);
    return NULL;
}

// main
int main(int argc, char *argv[]) {

    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        printf("Usage: %s -p <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[2]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket error");
        exit(1);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind error");
        exit(1);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen error");
        exit(1);
    }

    printf("s> init server 0.0.0.0:%d\n", port);
    printf("s>\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            perror("Accept error");
            continue;
        }

        // Crear hilo
        pthread_t thread;
        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;

        pthread_create(&thread, NULL, handle_client, pclient);
        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}