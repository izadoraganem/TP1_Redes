#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "common.h"

//função de logexit
void logexit(const char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

void usage(const char *progname){
    fprintf(stderr, "Uso: %s <ip> <porta_cliente> <porta_peer>\n", progname);
    fprintf(stderr, "Exemplo: %s 127.0.0.1 6000 6100\n", progname);
    exit(EXIT_FAILURE);
}

//inicializa endereço IPV4 
int addrparse(const char *ip_str, const char *port_str, struct sockaddr_in *addr){
    if(ip_str == NULL || port_str == NULL){
        return -1;
    }

    uint16_t port = (uint16_t)atoi(port_str);
    if(port == 0) return -1;  // Porta inválida

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

   inet_pton(AF_INET, ip_str, &(addr->sin_addr));

    return 0;
}

//printa o endereço
int addrtostr(const struct sockaddr *addr, char *str, size_t strsize){
    if(addr == NULL || str == NULL) return -1;

    void *numeric_addr = NULL;
    in_port_t port;

    if(addr->sa_family == AF_INET){
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        numeric_addr = &(addr4->sin_addr);
        port = ntohs(addr4->sin_port);
    } 
    if(inet_ntop(AF_INET, numeric_addr, str, (socklen_t)strsize) == NULL){
        return -1;
    }

    return 0;
}

//inicializa o socket do servidor
int server_socket_init(const char *portstr, struct sockaddr_in *addr4){
    uint16_t port = (uint16_t)atoi(portstr);
    if(port == 0){
        return -1;
    }

    memset(addr4, 0, sizeof(*addr4));
    addr4->sin_family = AF_INET;
    addr4->sin_port = htons(port);
    addr4->sin_addr.s_addr = INADDR_ANY; 

    return 0;
}

void gera_id(char id[11]){
    for (int i = 0; i < 10; i++){
        id[i] = '0' + (rand() % 10); 
    }
    id[10] = '\0';
}

int get_command_code(const char* buffer){
    if(strcmp(buffer, "Check failure") == 0) return 1;
    if(strncmp(buffer, "diagnose", 8) == 0) return 2;
     if(strncmp(buffer, "locate", 6) == 0) return 3;
    return 0; 
}