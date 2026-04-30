from enum import Enum
import argparse
import sys
import socket
import threading

class Client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _socket = None
    _listen_thread = None
    _current_user = None

    # ******************** METHODS *******************

    # *
    # * @brief Crea una conexión TCP al servidor y la devuelve.
    # * @return socket conectado al servidor
    @staticmethod
    def connectToServer():
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((Client._server, Client._port))
            return sock
        except Exception as e:
            print(f"Failed to connect to server: {e}")
            sys.exit(1)
 
    # *
    # * @brief Lee una cadena terminada en '\0' del socket dado.
    # * @param sock - socket del que leer
    # * @return cadena leída (sin el '\0' final)
    @staticmethod
    def _recv_string(sock):
        result = b""
        while True:
            byte = sock.recv(1)
            if not byte or byte == b'\x00':
                break
            result += byte
        return result.decode()
 
    # *
    # * @brief Hilo que escucha mensajes entrantes del servidor (SEND_MESSAGE y SEND_MESS_ACK).
    # *        Se ejecuta mientras el usuario esté conectado.
    # * @param listen_sock - socket en escucha donde el servidor se conectará
    @staticmethod
    def _hilo_escucha(listen_sock):
        listen_sock.settimeout(1.0)  # Timeout para poder salir cuando se cierre el socket
        while True:
            try:
                conn, addr = listen_sock.accept()
            except socket.timeout:
                # Comprobamos si el socket sigue abierto
                try:
                    listen_sock.fileno()  # lanza excepción si está cerrado
                    continue
                except Exception:
                    break  # Socket cerrado: terminamos el hilo
            except Exception:
                break  # Cualquier otro error cierra el hilo
 
            try:
                # Leemos la operación que nos envía el servidor
                operacion = Client._recv_string(conn)
                
                if operacion == "SEND_MESSAGE":
                    # El servidor nos entrega un mensaje de otro usuario
                    # Protocolo sección 8.6:
                    #   cadena: remitente
                    #   cadena: id del mensaje
                    #   cadena: texto del mensaje
                    remitente = Client._recv_string(conn)
                    msg_id    = Client._recv_string(conn)
                    mensaje   = Client._recv_string(conn)
                    print(f"\ns> MESSAGE {msg_id} FROM {remitente}")
                    print(f"  {mensaje}")
                    print("  END")
                    print("c> ", end="", flush=True)  # Restauramos el prompt
 
                elif operacion == "SEND_MESS_ACK":
                    # El servidor nos notifica que un mensaje que enviamos fue entregado
                    # Protocolo sección 8.6 (notificación al remitente):
                    #   cadena: id del mensaje entregado
                    msg_id = Client._recv_string(conn)
                    print(f"\nc> SEND MESSAGE {msg_id} OK")
                    print("c> ", end="", flush=True)
 
            except Exception as e:
                print(f"\nError en hilo de escucha: {e}")
            finally:
                conn.close()

    # *
    # * @param user - User name to register in the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def  register(user) :
        sock = Client.connectToServer()
        # Protocolo sección 8.1: enviamos "REGISTER\0" + nombre\0
        sock.sendall(("REGISTER\0").encode())
        sock.sendall((user + "\0").encode())    
        respuesta = sock.recv(1)
        sock.close()
        codigo = respuesta[0]
        if codigo == 0:
            print("REGISTER OK")
            return Client.RC.OK
        elif codigo == 1:
            print("USERNAME IN USE")
            return Client.RC.USER_ERROR
        else:
            print("REGISTER FAIL")
            return Client.RC.ERROR

    # *
    # 	 * @param user - User name to unregister from the system
    # 	 * 
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def  unregister(user) :
        sock = Client.connectToServer()
        # Protocolo sección 8.2: enviamos "UNREGISTER\0" + nombre\0
        sock.sendall(f"UNREGISTER\0".encode())
        sock.sendall((user + "\0").encode())
        respuesta = sock.recv(1)
        sock.close()
        codigo = respuesta[0]
        if codigo == 0:
            print("UNREGISTER OK")
            return Client.RC.OK
        elif codigo == 1:
            print("USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        else:
            print("UNREGISTER FAIL")
            return Client.RC.ERROR


    # *
    # * @param user - User name to connect to the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  connect(user) :
        # Creamos un socket local para escuchar los mensajes entrantes
        # Usamos el puerto 0 para que el OS nos asigne uno libre automáticamente
        listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_sock.bind(('0.0.0.0', 0)) 
        listen_sock.listen(5)
        puerto_asignado = listen_sock.getsockname()[1]
        
        # Arrancamos el hilo que se quedará escuchando en ese puerto
        Client._listen_thread = threading.Thread(
            target=Client._hilo_escucha, args=(listen_sock,), daemon=True
       )
        Client._listen_thread.start()
        
        sock = Client.connectToServer()
        # Protocolo sección 8.3: enviamos "CONNECT\0" + nombre\0 + puerto\0
        sock.sendall(f"CONNECT\0".encode())
        sock.sendall((user + "\0").encode())
        sock.sendall((str(puerto_asignado) + "\0").encode())
        respuesta = sock.recv(1)
        sock.close()
        
        codigo = respuesta[0]
        if codigo == 0:
            # Guardamos el socket en la clase para poder cerrarlo luego en el disconnect
            Client._socket = listen_sock
            # Guardamos el usuario conectado en la clase para usarlo en send y users
            Client._current_user = user
            print("CONNECT OK")
            return Client.RC.OK
        elif codigo == 1:
            listen_sock.close()
            print("CONNECT FAIL, USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        elif codigo == 2:
            listen_sock.close()
            print("USER ALREADY CONNECTED")
            return Client.RC.USER_ERROR
        else:
            listen_sock.close()
            print("CONNECT FAIL")
            return Client.RC.ERROR

    # *
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  users() :
        sock = Client.connectToServer()
        # Protocolo sección 8.7:
        #   "USERS\0" + current_user\0
        sock.sendall(("USERS\0").encode())
        sock.sendall((Client._current_user + "\0").encode())
 
        # Recibimos el código de resultado
        respuesta = sock.recv(1)
        codigo = respuesta[0]
        if codigo == 0:
            # Recibimos la cantidad de usuarios conectados (como cadena)
            num_str = Client._recv_string(sock)
            try:
                num_usuarios = int(num_str)
            except ValueError:
                sock.close()
                print("CONNECTED USERS FAIL")
                return Client.RC.ERROR
 
            # Recibimos los nombres de usuario (una cadena por usuario)
            usuarios = []
            for _ in range(num_usuarios):
                nombre = Client._recv_string(sock)
                usuarios.append(nombre)
            sock.close()
            print(f"CONNECTED USERS ({num_usuarios} users connected) OK")
            for u in usuarios:
                print(f"  {u}")
            return Client.RC.OK
 
        elif codigo == 1:
            sock.close()
            print("CONNECTED USERS FAIL, USER IS NOT CONNECTED")
            return Client.RC.USER_ERROR
        else:
            sock.close()
            print("CONNECTED USERS FAIL")
            return Client.RC.ERROR


    # *
    # * @param user - User name to disconnect from the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def  disconnect(user) :
        # Protocolo sección 8.4: "DISCONNECT\0" + nombre\0
        sock = Client.connectToServer()
        sock.sendall(("DISCONNECT\0").encode())
        sock.sendall((user + "\0").encode())
        respuesta = sock.recv(1)
        sock.close()
 
        # Cerramos el socket de escucha local para detener el hilo
        # independientemente del resultado
        if Client._socket:
            Client._socket.close()
            Client._socket = None
 
        Client._current_user = None  # Ya no hay usuario conectado
 
        codigo = respuesta[0]
        if codigo == 0:
            print("DISCONNECT OK")
            return Client.RC.OK
        elif codigo == 1:
            print("DISCONNECT FAIL, USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        elif codigo == 2:
            print("DISCONNECT FAIL, USER NOT CONNECTED")
            return Client.RC.USER_ERROR
        else:
            print("DISCONNECT FAIL")
            return Client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  send(user,  message) :
        sock = Client.connectToServer()
        # Protocolo sección 8.5:
        # "SEND\0" + remitente\0 + destinatario\0 + mensaje\0
        sock.sendall(("SEND\0").encode())
        sock.sendall((Client._current_user + "\0").encode())
        sock.sendall((user + "\0").encode())
        # El mensaje no puede superar 255 caracteres útiles (256 con el '\0')
        mensaje_truncado = message[:255]
        sock.sendall((mensaje_truncado + "\0").encode())
 
        # Recibimos el código de resultado (1 byte)
        respuesta = sock.recv(1)
        codigo = respuesta[0]
        if codigo == 0:
            # Éxito: leemos la cadena con el identificador del mensaje
            msg_id = Client._recv_string(sock)
            sock.close()
            print(f"SEND OK - MESSAGE {msg_id}")
            return Client.RC.OK
        elif codigo == 1:
            sock.close()
            print("SEND FAIL, USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        else:
            sock.close()
            print("SEND FAIL")
            return Client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param file    - file  to be sent
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  sendAttach(user,  file,  message) :
        #  Write your code here
        # parte 2
        return Client.RC.ERROR

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():
        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            Client.register(line[1])
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            Client.unregister(line[1])
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            Client.connect(line[1])
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            Client.disconnect(line[1])
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="USERS") :
                        if (len(line) == 1) :
                            Client.users()
                        else :
                            print("Syntax error. Usage: CONNECTED_USERS <userName>")

                    elif(line[0]=="SEND") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            message = ' '.join(line[2:])
                            Client.send(line[1], message)
                        else :
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif(line[0]=="SENDATTACH") :
                        if (len(line) >= 4) :
                            #  Remove first two words
                            message = ' '.join(line[3:])
                            Client.sendAttach(line[1], line[2], message)
                        else :
                            print("Syntax error. Usage: SENDATTACH <userName> <filename> <message>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            # Si hay un usuario conectado, lo desconectamos antes de salir
                            if Client._current_user is not None:
                                Client.disconnect(Client._current_user)
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")

    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False;
        
        Client._server = args.s
        Client._port = args.p

        return True

    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        if (not Client.parseArguments(argv)) :
            Client.usage()
            return

        #  Write code here
        Client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    Client.main([])