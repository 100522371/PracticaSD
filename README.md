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
