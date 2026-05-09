Práctica Final de Sistemas Distribuidos
**Autoras: Montserrat Martis Contreras, Marta Veber**

El proyecto está organizado de la siguiente manera:

├── Makefile              # Script de compilación
├── server.c              # Servidor de mensajería (cliente RPC)
├── server.h              # Definiciones de estructuras
├── client.py             # Cliente Python
├── web_service.py        # Servicio web de normalización
├── rpc/
│   ├── log.x             # Definición del servicio RPC
│   ├── log_server.c      # Implementación del servidor de logging
│   ├── ... otros archivos generados por rpcgen

Para compilar todo el proyecto se debe llamar a `make` en la terminal del directorio raíz. Este comando generará dos ejecutables:
    - `server`: Servidor de mensajería
    - `log_server`: Servidor de logging RPC

`make clean`: elimina solo los ejecutables

Antes de ejecutar client.py se debe ejecutar primero los tres otros componentes, cada uno en una terminal diferente.

1. Servicio Web
	$ python3 web_service.py

2. Servicio RPC
    ./log_server

3. Servicio de mensajería
    export LOG_RPC_IP=localhost
    ./server -p <puerto servidor>

Después de ejecutar los servicios se puede ejecutar client.py:
    python3 client.py -s <ip> -p <puerto servidor>
