#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "server.h"
//#include "log.h"

#define MAX_USERS 100

// Definición de las variables globales de usuarios
User users[MAX_USERS];
int num_users = 0;

// Inicialización del mutex global
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

// Devolver índice del user o -1 si no existe
int find_user(char *username) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

// Añadir un nuevo usuario
int add_user(char *username) {
    if (num_users >= MAX_USERS) {
        return -1; // Error: Límite de usuarios alcanzado
    }
    strcpy(users[num_users].username, username);
    users[num_users].connected = 0;
    users[num_users].port = 0;
    users[num_users].ip[0] = '\0';
    users[num_users].num_pending = 0;
    users[num_users].last_msg_id = 0;
    num_users++;
    return 0; // Éxito
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
    char username[50];
    char filename[256];

    recv_string(client_sock, operation);
    if (strcmp(operation, "REGISTER") == 0) {
        pthread_mutex_lock(&users_mutex);
        recv_string(client_sock, username);
        if (user_exists(username)) {
            // Usuario ya existe
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> REGISTER %s FAIL\n", username);

        }
        // usuario nuevo
        int result = add_user(username);
        if (result == 0) {
            char code = 0;
            send(client_sock, &code, 1, 0);
            printf("s> REGISTER %s OK\n", username);
        } else { // Error 
            char code = 2;
            send(client_sock, &code, 1, 0);
            printf("s> REGISTER %s FAIL\n", username);
        }
        pthread_mutex_unlock(&users_mutex);


    } else if (strcmp(operation, "UNREGISTER") == 0) {
        pthread_mutex_lock(&users_mutex);
        recv_string(client_sock, username);
        int idx = find_user(username);
        if (idx != -1) {
            //limpiar mensajes pendientes
            for (int i = 0; i < users[idx].num_pending; i++) {
                memset(&users[idx].pending[i], 0, sizeof(Message));
            }
            users[idx].num_pending = 0;
        }
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
        pthread_mutex_lock(&users_mutex);
        
        char port_str[20];
        recv_string(client_sock, username);
        recv_string(client_sock, port_str);

        int port = atoi(port_str);
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
            int new_count = 0;
            for (int i = 0; i < u->num_pending; i++) {
                int enviado = 0;
                int sock_dest = socket(AF_INET, SOCK_STREAM, 0);

                struct sockaddr_in dest_addr;
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(u->port);
                inet_pton(AF_INET, u->ip, &dest_addr.sin_addr);

                if (connect(sock_dest, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == 0) {

                    //Enviar mensaje
                    send(sock_dest, "SEND MESSAGE\0", strlen("SEND MESSAGE") + 1, 0);
                    send(sock_dest, u->pending[i].sender, strlen(u->pending[i].sender) + 1, 0);

                    char id_str[20];
                    sprintf(id_str, "%u", u->pending[i].id);
                    send(sock_dest, id_str, strlen(id_str) + 1, 0);
                    send(sock_dest, u->pending[i].text, strlen(u->pending[i].text) + 1, 0);

                    printf("s> SEND MESSAGE %u FROM %s TO %s\n", u->pending[i].id, u->pending[i].sender, username);
                    enviado = 1;

                    //Añadir ACK al emisor
                    int sender_idx = find_user(u->pending[i].sender);

                    if (sender_idx != -1 && users[sender_idx].connected) {

                        int sock_sender = socket(AF_INET, SOCK_STREAM, 0);
                        struct sockaddr_in sender_addr;
                        sender_addr.sin_family = AF_INET;
                        sender_addr.sin_port = htons(users[sender_idx].port);
                        inet_pton(AF_INET, users[sender_idx].ip, &sender_addr.sin_addr);

                        if (connect(sock_sender, (struct sockaddr *)&sender_addr, sizeof(sender_addr)) == 0) {

                            send(sock_sender, "SEND MESS ACK\0", strlen("SEND MESS ACK") + 1, 0);
                            char id_str_ack[20];
                            sprintf(id_str_ack, "%u", u->pending[i].id);
                            send(sock_sender, id_str_ack, strlen(id_str_ack) + 1, 0);
                        }
                        close(sock_sender);
                    }

                } else {
                    //Si falla marcar como desconectado
                    users[idx].connected = 0;
                }

                close(sock_dest);

                //Mantenemos los mensajes no enviados
                if (!enviado) {
                    u->pending[new_count++] = u->pending[i];
                }
            }
            // Actualizar cola
            u->num_pending = new_count;
        }
        pthread_mutex_unlock(&users_mutex);


    } else if (strcmp(operation, "DISCONNECT") == 0) {
        pthread_mutex_lock(&users_mutex);
        recv_string(client_sock, username);
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
            // Limpiar datos de conexión de usuario que estaba conectado
            users[idx].connected = 0;
            users[idx].port = 0;
            users[idx].ip[0] = '\0';

            char code = 0;
            send(client_sock, &code, 1, 0);
            printf("s> DISCONNECT %s OK\n", username);
        }
        pthread_mutex_unlock(&users_mutex);


    } else if (strcmp(operation, "SEND") == 0) {
        pthread_mutex_lock(&users_mutex);
        char receiver[50];
        char message[BUFFER_SIZE];

        recv_string(client_sock, username);
        recv_string(client_sock, receiver);
        recv_string(client_sock, message);

        int sender_idx = find_user(username);
        int receiver_idx = find_user(receiver);
        if (sender_idx == -1 || receiver_idx == -1) {
            // emisor o receptor no existe
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> SEND FAIL, USER DOES NOT EXIST\n");

        } else {
            // Contador por usuario
            users[sender_idx].last_msg_id++;
            unsigned int msg_id = users[sender_idx].last_msg_id;

            Message m;
            strcpy(m.sender, username);
            strcpy(m.text, message);
            m.id = msg_id;
            m.has_attachment = 0;
            m.fileName[0] = '\0';

            User *recv_user = &users[receiver_idx];
            // Guardar en pendientes
            if (recv_user->num_pending < 100) {
                recv_user->pending[recv_user->num_pending++] = m;
            }
            //Si el receptor está desconectado, el mensaje queda alamacenado
            if (!users[receiver_idx].connected) {
                printf("s> MESSAGE %u FROM %s TO %s STORED\n", msg_id, username, receiver);
            }

            char code = 0;
            send(client_sock, &code, 1, 0);

            // Enviar ID como string terminado en \0
            char id_str[20];
            sprintf(id_str, "%u", msg_id);
            send(client_sock, id_str, strlen(id_str) + 1, 0);

            printf("s> SEND MESSAGE %u FROM %s TO %s\n", msg_id, username, receiver);

            // Enviar mensaje a receptor si está conectado
            if (users[receiver_idx].connected) {

                int sock_dest = socket(AF_INET, SOCK_STREAM, 0);

                struct sockaddr_in dest_addr;
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(users[receiver_idx].port);
                inet_pton(AF_INET, users[receiver_idx].ip, &dest_addr.sin_addr);

                if (connect(sock_dest, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == 0) {

                    //enviar protocolo servidor-cliente
                    send(sock_dest, "SEND MESSAGE\0", strlen("SEND MESSAGE") + 1, 0);
                    send(sock_dest, username, strlen(username) + 1, 0);

                    send(sock_dest, id_str, strlen(id_str) + 1, 0);
                    send(sock_dest, message, strlen(message) + 1, 0);

                    // ELiminamos el mensaje de la cola
                    for (int j = 0; j < recv_user->num_pending - 1; j++) {
                        recv_user->pending[j] = recv_user->pending[j + 1];
                    }
                    recv_user->num_pending--;
                } else {
                    //si falla conexión a receptor, consideramos desconectado
                    users[receiver_idx].connected = 0;
                    users[receiver_idx].port = 0;
                    users[receiver_idx].ip[0] = '\0';
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
                        send(sock_sender, "SEND MESS ACK\0", strlen("SEND MESS ACK") + 1, 0);
                        send(sock_sender, id_str, strlen(id_str) + 1, 0);
                    }
                    close(sock_sender);
                }
            } 
        }
        pthread_mutex_unlock(&users_mutex);


    } else if (strcmp(operation, "SENDATTACH") == 0) {
        pthread_mutex_lock(&users_mutex);
        char receiver[50];
        char message[BUFFER_SIZE];

        recv_string(client_sock, username);
        recv_string(client_sock, receiver);
        recv_string(client_sock, message);
        recv_string(client_sock, filename);

        int sender_idx = find_user(username);
        int receiver_idx = find_user(receiver);

        if (sender_idx == -1 || receiver_idx == -1) {
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> SENDATTACH FAIL, USER DOES NOT EXIST\n");

        } else {
            // generamos ID
            users[sender_idx].last_msg_id++;
            unsigned int msg_id = users[sender_idx].last_msg_id;

            // crear el mensaje
            Message m;
            strcpy(m.sender, username);
            strcpy(m.text, message);
            m.id = msg_id;
            m.has_attachment = 1;
            strcpy(m.fileName, filename);

            // guardar en pendientes
            User *recv_user = &users[receiver_idx];
            if (recv_user->num_pending < 100) {
                recv_user->pending[recv_user->num_pending++] = m;
            }
            // almacenar si está desconectado
            if (!users[receiver_idx].connected) {
                printf("s> MESSAGE %u FROM %s TO %s FILE %s STORED\n", msg_id, username, receiver, filename);
            }
            char code = 0;
            send(client_sock, &code, 1, 0);

            char id_str[20];
            sprintf(id_str, "%u", msg_id);
            send(client_sock, id_str, strlen(id_str) + 1, 0);

            printf("s> SENDATTACH MESSAGE %u FROM %s TO %s FILE %s\n", msg_id, username, receiver, filename);

            if (users[receiver_idx].connected) {
                int sock_dest = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in dest_addr;
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(users[receiver_idx].port);
                inet_pton(AF_INET, users[receiver_idx].ip, &dest_addr.sin_addr);

                if (connect(sock_dest, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == 0) {

                    send(sock_dest, "SEND MESSAGE ATTACH\0", strlen("SEND MESSAGE ATTACH") + 1, 0);
                    send(sock_dest, username, strlen(username) + 1, 0);

                    send(sock_dest, id_str, strlen(id_str) + 1, 0);
                    send(sock_dest, message, strlen(message) + 1, 0);
                    send(sock_dest, filename, strlen(filename) + 1, 0);

                    //Eliminamos el mensaje de pendientes
                    for (int j = 0; j < recv_user->num_pending - 1; j++) {
                        recv_user->pending[j] = recv_user->pending[j + 1];
                    }
                    recv_user->num_pending--;

                } else {
                    //marcar como desconectado si falla
                    users[receiver_idx].connected = 0;
                    users[receiver_idx].port = 0;
                    users[receiver_idx].ip[0] = '\0';
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
                        send(sock_sender, "SEND MESS ATTACH ACK\0", strlen("SEND MESS ATTACH ACK") + 1, 0);
                        send(sock_sender, id_str, strlen(id_str) + 1, 0);
                        send(sock_sender, filename, strlen(filename) + 1, 0);
                    }
                    close(sock_sender);
                }
            }    
        }
        pthread_mutex_unlock(&users_mutex);


    } else if (strcmp(operation, "USERS") == 0) {
        pthread_mutex_lock(&users_mutex);
        recv_string(client_sock, username);
        int idx = find_user(username);
        if (idx == -1) {
            // Usuario no existe
            char code = 2;
            send(client_sock, &code, 1, 0);
            printf("s> CONNECTEDUSERS FAIL\n");

        } else if (!users[idx].connected) {
            //Usuario no conectado
            char code = 1;
            send(client_sock, &code, 1, 0);
            printf("s> CONNECTEDUSERS FAIL\n");
            
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

            // Enviar cadenas de usuarios conectados
            // usuario :: ip :: puerto
            for (int i = 0; i < num_users; i++) {
                if (users[i].connected) {
                    char user_info[300];
                    sprintf(user_info, "%s::%s::%d", users[i].username, users[i].ip, users[i].port);
                    send(client_sock, user_info, strlen(user_info) + 1, 0);
                }
            }
            printf("s> CONNECTEDUSERS OK\n");
        }
        pthread_mutex_unlock(&users_mutex);


    } else {
        // si otra operación 
        printf("s> UNKNOWN OPERATION\n");
    }

    //struct log_args log_data = {
    //    username = username,
    //    op = operation,
    //    filename = filename
    //};
    //log_1(log_data, NULL, NULL);
    
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