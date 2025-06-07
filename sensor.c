#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.h"

#define BUFFER_SIZE 501

void usage(int argc, char **argv){
    fprintf(stderr, "Uso: %s <server IP> <porta servidor1> <porta servidor2>\n", argv[0]);
    fprintf(stderr, "Exemplo: %s 127.0.0.1 6000 6100\n", argv[0]);
    exit(EXIT_FAILURE);
}

void comunicacao(const char *ip, int porta){

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(porta);

    if(inet_pton(AF_INET, ip, &addr.sin_addr) <= 0){
        logexit("inet_pton");
    }

    int s;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1){
        logexit("socket");
    }

    if(connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0){
        logexit("connect");
    }

    char addrstr[INET_ADDRSTRLEN + 1];
    addrtostr((struct sockaddr *)&addr, addrstr, sizeof(addrstr));
    printf("Conectado ao servidor %s (porta %d)\n", addrstr, porta);

    char buffer[BUFFER_SIZE];
    int count = 0;
    int total = 0;

    while((count = recv(s, buffer, sizeof(buffer) - 1, 0)) > 0){
        buffer[count] = '\0'; // garantir terminação nula para strings
        printf("Recebido %d bytes do servidor %s:\n%s\n", count, addrstr, buffer);
        total += count;
    }

    if(count == -1){
        logexit("recv");
    }

    close(s);
    printf("Conexão com %s (porta %d) encerrada. Total recebido: %d bytes\n\n", addrstr, porta, total);
}

int main(int argc, char **argv){
    if(argc != 4){
        usage(argc, argv);
    }

    const char *ip = argv[1];
    int porta_servidorSL = atoi(argv[2]);
    int porta_servidorSS = atoi(argv[3]);

    comunicacao(ip, porta_servidorSL);
    comunicacao(ip, porta_servidorSS);

    return EXIT_SUCCESS;
}
