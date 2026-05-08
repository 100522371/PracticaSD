# Compilador y flags
CC = gcc
CFLAGS = -Wall -g -fPIC -Iinclude -I$(RPC) -I/usr/include/tirpc
LDFLAGS = -lpthread -ltirpc
RPCGEN = rpcgen

# Directorios y archivos
RPC = rpc
RPC_SRC = $(RPC)/log.x

# Archivos generados por rpcgen
RPC_FILES = \
    $(RPC)/log_clnt.c \
    $(RPC)/log_svc.c \
    $(RPC)/log_xdr.c \
    $(RPC)/log.h

# Archivos fuente
SERVER_SRC = server.c $(RPC)/log_clnt.c $(RPC)/log_xdr.c
LOG_SERVER_SRC = $(RPC)/log_server.c $(RPC)/log_svc.c $(RPC)/log_xdr.c

# Targets principales
all: server log_server

# Generar código RPC desde log.x
$(RPC_FILES): $(RPC_SRC)
	$(RPCGEN) -C $(RPC_SRC) -i $(RPC)

# Compilar servidor de mensajería
server: $(RPC_FILES) $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDFLAGS)

# Compilar servidor de logging
log_server: $(RPC_FILES) $(LOG_SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $(LOG_SERVER_SRC) $(LDFLAGS)

# Limpiar ejecutables
clean:
	rm -f server log_server

# Limpiar TODO incluyendo archivos generados por rpcgen
distclean: clean
	rm -f $(RPC_FILES)

.PHONY: all clean distclean