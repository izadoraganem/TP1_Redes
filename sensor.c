#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.h"

#define BUFFER_SIZE 501

#define REQ_CONNSEN 23
#define RES_CONNSEN 24
#define ERROR 255

void usage(int argc, char **argv){
    fprintf(stderr, "Uso: %s <server IP> <porta servidor1> <porta servidor2>\n", argv[0]);
    fprintf(stderr, "Exemplo: %s 127.0.0.1 6000 6100\n", argv[0]);
    exit(EXIT_FAILURE);
}

void comunicacao(const char *ip, const char *porta, char *id){
    if(strcmp(porta, "60000") == 0){
        printf("Iniciando comunicação com o servidor SL na porta %s...\n", porta);
        int s_SL = socket(AF_INET, SOCK_STREAM, 0);
        if(s_SL == -1){
        logexit("socket SL");
        }

        struct sockaddr_in addr;
        if(addrparse(ip, porta, &addr) != 0){
            logexit("addrparse");
        }
        
        if(connect(s_SL, (struct sockaddr *)&addr, sizeof(addr)) != 0){
            // close(s_SL);
            logexit("connect");
        }

        //cliente requisita conexao e envia seu id
        char requisicao[BUFFER_SIZE];
        memset(requisicao, 0, sizeof(requisicao));
        requisicao[0] = REQ_CONNSEN; 
        strncpy(&requisicao[1], id, 10);
        send(s_SL, requisicao, sizeof(requisicao), 0);
        memset(requisicao, 0, sizeof(requisicao));

        char resposta[BUFFER_SIZE];
        recv(s_SL, resposta, sizeof(resposta), 0);

        //se o limite de conexoes foi alcancado imprime o erro
        if(((unsigned char)resposta[0] == ERROR) && (resposta[1] == '9')){
            logexit("Sensor limit exceeded");
        }else{
            //consegue conectar então imprime seu id
            if(resposta[0] == RES_CONNSEN){
                printf("SL New ID: %s\n", id);
            }
        }
    }
    
    if(strcmp(porta, "61000") == 0){
        printf("Iniciando comunicação com o servidor SS na porta %s...\n", porta);
        int s_SS = socket(AF_INET, SOCK_STREAM, 0);
        if(s_SS == -1){
        logexit("socket SS");
        }

        struct sockaddr_in addr;
        if(addrparse(ip, porta, &addr) != 0){
            logexit("addrparse");
        }
        
        if(connect(s_SS, (struct sockaddr *)&addr, sizeof(addr)) != 0){
            // close(s_SS);
            logexit("connect");
        }

        //cliente requisita conexao e envia seu id
        char requisicao[BUFFER_SIZE];
        memset(requisicao, 0, sizeof(requisicao));
        requisicao[0] = REQ_CONNSEN; 
        strncpy(&requisicao[1], id, 10);
        send(s_SS, requisicao, sizeof(requisicao), 0);
        memset(requisicao, 0, sizeof(requisicao));

        char resposta[BUFFER_SIZE];
        recv(s_SS, resposta, sizeof(resposta), 0);

        //se o limite de conexoes foi alcancado imprime o erro
        if(((unsigned char)resposta[0] == ERROR) && (resposta[1] == '9')){
            logexit("Sensor limit exceeded");
        }else{
            //consegue conectar então imprime seu id
            if(resposta[0] == RES_CONNSEN){
                printf("SS New ID: %s\n", id);
            }
        }
    }

}


int main(int argc, char **argv){
    if(argc != 4){
        usage(argc, argv);
    }

    const char *ip = argv[1];
    const char *porta_servidorSL = argv[2];
    const char *porta_servidorSS = argv[3];

    char id[11];

    gera_id(id);

    comunicacao(ip, porta_servidorSL, id);
    comunicacao(ip, porta_servidorSS, id);

    fd_set read_fds;
    int max_fd;

    while(1){
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        
    }

    return EXIT_SUCCESS;
}
