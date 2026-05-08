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
    _socket = None # Socket local de escucha para mensajes entrantes del servidor
    _listen_thread = None
    _current_user = None
    _usuarios = [] # Lista de usuarios conectados (nombre, ip, puerto)

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
    # * @brief Llama al servicio web de normalización para normalizar un mensaje.
    # * @param message - texto a normalizar
    # * @return cadena normalizada (o el mensaje original si hubo un error)
    @staticmethod
    def normalize_message(message):
        try:
            import requests
            url = "http://127.0.0.1:8080/normalize"
            response = requests.post(url, data=message)

            if response.status_code == 200:
                return response.text
            else:
                print("ERROR: Web service failed")
                return message
        
        except Exception as e:
            print(f"ERROR calling web service: {e}")
            return message

    # *
    # * @brief Hilo que escucha mensajes entrantes del servidor (SEND MESSAGE y SEND MESS ACK).
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
                
                if operacion == "SEND MESSAGE":
                    # El servidor nos entrega un mensaje de otro usuario
                    remitente = Client._recv_string(conn)
                    msg_id    = Client._recv_string(conn)
                    mensaje   = Client._recv_string(conn)
                    print(f"\ns> MESSAGE {msg_id} FROM {remitente}")
                    print(f"  {mensaje}")
                    print("  END")
                    print("c> ", end="", flush=True)  # Restauramos el prompt
 
                elif operacion == "SEND MESS ACK":
                    # El servidor nos notifica que un mensaje que enviamos fue entregado
                    msg_id = Client._recv_string(conn)
                    print(f"\nc> SEND MESSAGE {msg_id} OK")
                    print("c> ", end="", flush=True)

                elif operacion == "SEND MESSAGE ATTACH":
                    remitente = Client._recv_string(conn)
                    msg_id    = Client._recv_string(conn)
                    mensaje   = Client._recv_string(conn)
                    filename  = Client._recv_string(conn)

                    print(f"\ns> MESSAGE {msg_id} FROM {remitente}")
                    print(f"  {mensaje}")
                    print("  END")
                    print(f"  FILE {filename}")
                    print("c> ", end="", flush=True)
                
                elif operacion == "SEND MESS ATTACH ACK":
                    msg_id = Client._recv_string(conn)
                    filename = Client._recv_string(conn)
                    print(f"\nc> SENDATTACH MESSAGE {msg_id} {filename} OK")
                    print("c> ", end="", flush=True)
                
                elif operacion == "GETFILE":
                    # usuario que nos pide el archivo
                    user = Client._recv_string(conn)
                    filename = Client._recv_string(conn)
                    print(f"\nc> GETFILE {filename} REQUEST FROM {user}")

                    # obtenemos su ip y puerto de escucha
                    for i in Client._usuarios:
                        if i[0] == user:
                            ip = i[1]
                            puerto = int(i[2])
                    
                    if ip is None or puerto is None:
                        print(f"c> FILE TRANSFER FAILED, user not connected")
                        return Client.RC.USER_ERROR
                    try:
                        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        sock.connect((ip, puerto))
                    except Exception as e:
                        print(f"c> FILE TRANSFER FAILED, could not connect to user {user}: {e}")
                        return Client.RC.ERROR
                    
                    # Enviamos el contenido del archivo
                    try:
                        with open(filename, 'r') as f:
                            file_content = f.read()
                        sock.sendall((file_content + "\0").encode())
                        print(f"c> FILE {filename} SENT TO {user}")
                    except Exception as e:
                        print(f"c> FILE TRANSFER FAILED, could not send file {filename} to user {user}: {e}")
                        return Client.RC.ERROR

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
        # enviamos "REGISTER\0" + nombre\0
        sock.sendall(("REGISTER\0").encode())
        sock.sendall((user + "\0").encode())    
        respuesta = sock.recv(1)
        sock.close()
        codigo = respuesta[0]
        if codigo == 0:
            print("c> REGISTER OK")
            return Client.RC.OK
        elif codigo == 1:
            print("c> USERNAME IN USE")
            return Client.RC.USER_ERROR
        else:
            print("c> REGISTER FAIL")
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
        # enviamos "UNREGISTER\0" + nombre\0
        sock.sendall(f"UNREGISTER\0".encode())
        sock.sendall((user + "\0").encode())
        respuesta = sock.recv(1)
        sock.close()
        codigo = respuesta[0]
        if codigo == 0:
            print("c> UNREGISTER OK")
            return Client.RC.OK
        elif codigo == 1:
            print("c> USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        else:
            print("c> UNREGISTER FAIL")
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
        # enviamos "CONNECT\0" + nombre\0 + puerto\0
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
            print("c> CONNECT OK")
            return Client.RC.OK
        elif codigo == 1:
            listen_sock.close()
            print("c> CONNECT FAIL, USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        elif codigo == 2:
            listen_sock.close()
            print("c> USER ALREADY CONNECTED")
            return Client.RC.USER_ERROR
        else:
            listen_sock.close()
            print("c> CONNECT FAIL")
            return Client.RC.ERROR

    # *
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  users() :
        if Client._current_user is None:
            print("c> ERROR: NOT CONNECTED")
            return Client.RC.USER_ERROR
        
        sock = Client.connectToServer()
        # "USERS\0" + current_user\0
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
                print("c> CONNECTED USERS FAIL")
                return Client.RC.ERROR
            
            # Recibimos una cadena por usuario
            # sendattach: usuario :: ip :: puerto
            for _ in range(num_usuarios):
                cadena = Client._recv_string(sock)
                usuario, ip, puerto = cadena.split("::")
                Client._usuarios.append((usuario, ip, puerto))
            sock.close()
            print(f"c> CONNECTED USERS ({num_usuarios} users connected) OK")
            for u in Client._usuarios:
                print(f"  {u[0]} :: {u[1]} :: {u[2]}")
            return Client.RC.OK
 
        elif codigo == 1:
            sock.close()
            print("c> CONNECTED USERS FAIL, USER IS NOT CONNECTED")
            return Client.RC.USER_ERROR
        else:
            sock.close()
            print("c> CONNECTED USERS FAIL")
            return Client.RC.ERROR


    # *
    # * @param user - User name to disconnect from the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def  disconnect(user) :
        # "DISCONNECT\0" + nombre\0
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
            print("c> DISCONNECT OK")
            return Client.RC.OK
        elif codigo == 1:
            print("c> DISCONNECT FAIL, USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        elif codigo == 2:
            print("c> DISCONNECT FAIL, USER NOT CONNECTED")
            return Client.RC.USER_ERROR
        else:
            print("c> DISCONNECT FAIL")
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
        if Client._current_user is None:
            print("c> ERROR: NOT CONNECTED")
            return Client.RC.USER_ERROR
        
        sock = Client.connectToServer()

        # "SEND\0" + remitente\0 + destinatario\0 + mensaje\0
        sock.sendall(("SEND\0").encode())
        sock.sendall((Client._current_user + "\0").encode())
        sock.sendall((user + "\0").encode())
        # Normalizamos el mensaje usando el servicio web
        normalized_message = Client.normalize_message(message)
        # Truncamos el mensaje a 255 caracteres para evitar problemas de tamaño
        mensaje_truncado = normalized_message[:255]
        sock.sendall((mensaje_truncado + "\0").encode())
 
        # Recibimos el código de resultado (1 byte)
        respuesta = sock.recv(1)
        codigo = respuesta[0]
        if codigo == 0:
            # Éxito: leemos la cadena con el identificador del mensaje
            msg_id = Client._recv_string(sock)
            sock.close()
            print(f"c> SEND OK - MESSAGE {msg_id}")
            return Client.RC.OK
        elif codigo == 1:
            sock.close()
            print("c> SEND FAIL, USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        else:
            sock.close()
            print("c> SEND FAIL")
            return Client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param file    - file to be sent
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  sendAttach(user, message, file) :
        if Client._current_user is None:
            print("c> ERROR: NOT CONNECTED")
            return Client.RC.USER_ERROR
        sock = Client.connectToServer()
        
        # "SENDATTACH\0" + remitente\0 + destinatario\0 + mensaje\0 + filename\0
        sock.sendall(("SENDATTACH\0").encode())
        sock.sendall((Client._current_user + "\0").encode())
        print(f"DEBUG: Sent remitente")

        sock.sendall((user + "\0").encode())
        print(f"DEBUG: Sent user={user}")

        # Normalizamos el mensaje usando el servicio web
        normalized_message = Client.normalize_message(message)
        # Truncamos el mensaje a 255 caracteres para evitar problemas de tamaño
        mensaje_truncado = normalized_message[:255]
        sock.sendall((mensaje_truncado).encode())
        print(f"DEBUG: Sent message={mensaje_truncado}")
        sock.sendall((file + "\0").encode())
        print(f"DEBUG: Sent filename={file}")

        # Recibimos el código de resultado (1 byte)
        respuesta = sock.recv(1)
        codigo = respuesta[0]
        if codigo == 0:
            # Éxito: leemos la cadena con el identificador del mensaje
            msg_id = Client._recv_string(sock)
            sock.close()
            print(f"c> SENDATTACH OK - MESSAGE {msg_id}")
            return Client.RC.OK
        elif codigo == 1:
            sock.close()
            print("c> SENDATTACH FAIL, USER DOES NOT EXIST")
            return Client.RC.USER_ERROR
        else:
            sock.close()
            print("c> SENDATTACH FAIL")
            return Client.RC.ERROR
    
    # *
    # * @param user       - sender username
    # * @param file       - file to be transferred
    # * @param local_file - local file where the file will be copied
    # * 
    # * @return OK if the file was successfully received and saved
    # * @return USER_ERROR if the user is not connected or does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def getfile(user, file, local_file):
        if Client._current_user is None:
            print("c> ERROR: NOT CONNECTED")
            return Client.RC.USER_ERROR
        
        Client.users()  # Actualizamos la lista de usuarios conectados
        for i in Client._usuarios:
            if i[0] == user:
                ip = i[1]
                puerto = int(i[2])
        if ip is None or puerto is None:
            print("c> FILE TRANSFER FAILED, user not connected")
            return Client.RC.USER_ERROR
        
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((ip, puerto))
        except Exception as e:
            print(f"c> FILE TRANSFER FAILED, could not connect to user {user}: {e}")
        
        # Enviamos "GETFILE\0" + filename\0
        sock.sendall(("GETFILE\0").encode())
        sock.sendall((Client._current_user + "\0").encode())
        sock.sendall((file + "\0").encode())

        # Leemos el contenido del archivo
        file_content = Client._recv_string(sock)
        sock.close()
        try:
            with open(local_file, 'w') as f:
                f.write(file_content)
            print(f"c> FILE {file} RECEIVED AND SAVED AS {local_file}")
            return Client.RC.OK
        except Exception as e:
            print(f"c> FILE TRANSFER FAILED, could not save file: {e}")
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
                            print("Syntax error. Usage: USERS")

                    elif(line[0]=="SEND") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            message = ' '.join(line[2:])
                            Client.send(line[1], message)
                        else :
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif(line[0]=="SENDATTACH") :
                        if (len(line) >= 4) :
                            # filename sera la ultima palabra, el mensaje todo lo que haya entre el destinatario y la última palabra
                            filename = line[-1]
                            message = ' '.join(line[2:-1])
                            print(f"DEBUG: user={line[1]}, message={message}, filename={filename}")
                            Client.sendAttach(line[1], message, filename)
                        else :
                            print("Syntax error. Usage: SENDATTACH <userName> <message> <filename>")
                    
                    elif (line[0]=="GETFILE") :
                        if (len(line) == 4) :
                            Client.getfile(line[1], line[2], line[3])
                        else :
                            print("Syntax error. Usage: GETFILE <userName> <filename> <localFilename>")

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

        Client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    Client.main([])