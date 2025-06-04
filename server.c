#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "common.h"

#define MAX_CLIENTES 15
#define BUFFER_SIZE 1024

void usage(const char *progname) {
    fprintf(stderr, "Uso: %s <ip> <porta_cliente> <porta_peer>\n", progname);
    fprintf(stderr, "Exemplo: %s 127.0.0.1 6000 6100\n", progname);
    exit(EXIT_FAILURE);
}

void cliente_servidor(const char *ip, const char *porta_cliente) {
    struct sockaddr_in addr4;

    if (addrparse(ip, porta_cliente, &addr4) != 0) {
        fprintf(stderr, "Erro ao analisar IP/porta\n");
        exit(EXIT_FAILURE);
    }

    if (server_socket_init("tcp", porta_cliente, &addr4) != 0) {
        fprintf(stderr, "Erro ao inicializar socket do servidor\n");
        exit(EXIT_FAILURE);
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    if (bind(s, (struct sockaddr *)&addr4, sizeof(addr4)) != 0) {
        logexit("bind");
    }

    if (listen(s, MAX_CLIENTES) != 0) {
        logexit("listen");
    }

    printf("Servidor ouvindo em %s:%s\n", ip, porta_cliente);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int socket_cliente = accept(s, (struct sockaddr *)&client_addr, &client_addr_len);
        if (socket_cliente == -1) {
            logexit("accept");
            continue;
        }

        char client_addr_str[INET_ADDRSTRLEN + 1];
        addrtostr((struct sockaddr *)&client_addr, client_addr_str, sizeof(client_addr_str));
        printf("Cliente conectado: %s\n", client_addr_str);

        char buffer[BUFFER_SIZE];
        ssize_t count = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
        if (count < 0) {
            logexit("recv");
            close(socket_cliente);
            continue;
        }

        buffer[count] = '\0'; // Termina a string recebida
        printf("Recebido %zd bytes do cliente: %s\n", count, buffer);

        send(socket_cliente, buffer, strlen(buffer), 0); // Eco
        close(socket_cliente);
    }

    close(s);
}


void peer2peer(const char *ip, const char *porta_peer) {
    // Implementação futura
}

int main(int argc, char **argv) {
    if (argc != 4) {
        usage(argv[0]);
    }

    const char *ip = argv[1];
    const char *porta_cliente = argv[2];
    const char *porta_peer = argv[3];

    if (atoi(porta_cliente) <= 0 || atoi(porta_peer) <= 0) {
        usage(argv[0]);
    }

    printf("Servidor iniciado com IP %s\n", ip);
    printf("Porta cliente: %s\n", porta_cliente);
    printf("Porta peer: %s\n", porta_peer);

    cliente_servidor(ip, porta_cliente);
    // peer2peer(ip, porta_peer);

    return 0;
}
