#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "common.h"

#define BUFFER_SIZE 1024

void usage (int argc, char **argv){
    fprintf(stderr, "Uso: %s <server IP> <server port>\n", argv[0]);
    fprintf(stderr, "Exemplo: %s 127.0.0.1 60000\n", argv[0]);
    exit(EXIT_FAILURE);
}


int main(int arc, char **argv){
    if (arc != 3) {
        usage(arc, argv);
    }

    int s;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));

    if(inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0){
        logexit("inet_pton");
    }

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    if(connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0){
    logexit("connect");
    }


    char addrstr[INET_ADDRSTRLEN + 1];
    addrtostr((struct sockaddr *)&addr, addrstr, sizeof(addrstr));
    printf("Conectado ao servidor %s\n", addrstr);

    char buffer[BUFFER_SIZE];
    unsigned total = 0;
    int count = 0;

    while(1){
        count = recv(s, buffer + total, sizeof(buffer) - total, 0);
        if(count == 0){
            break; //terminou a conexão
        }

        total += count;
    }

    close(s);

    printf("Recebido %d bytes do servidor %s\n", total, addrstr);
    fwrite(buffer, 1, total, stdout);


    exit(EXIT_SUCCESS);
}