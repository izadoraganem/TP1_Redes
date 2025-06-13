#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.h"
#include <time.h>

#define BUFFER_SIZE 501

//mensagens
#define REQ_CONNSEN 23
#define RES_CONNSEN 24
#define REQ_DISCSEN 25

#define REQ_SENSLOC 38
#define RES_SENSLOC 39

#define REQ_SENSSTATUS 40
#define RES_SENSSTATUS 41
#define REQ_LOCLIST 42
#define RES_LOCLIST 43


#define ERROR 255
#define OK 0

//variaveis globais
int s_SL;
int s_SS;
char id[11];
fd_set read_fds;
int max_fd;

//conexao com o servidor
void conexao(const char *ip, const int porta, char *id){
    //conexao com SL
    if(porta == 60000){
        s_SL = socket(AF_INET, SOCK_STREAM, 0);
        if(s_SL == -1){
            logexit("socket SL");
        }

        struct sockaddr_in addr;
        char portac[10];
        sprintf(portac, "%d", porta);
        if(addrparse(ip, portac, &addr) != 0){
            logexit("addrparse");
        }

        if(connect(s_SL, (struct sockaddr *)&addr, sizeof(addr)) != 0){
            logexit("connect SL");
        }

        char requisicao[BUFFER_SIZE];
        memset(requisicao, 0, sizeof(requisicao));
        requisicao[0] = REQ_CONNSEN; 
        strncpy(&requisicao[1], id, 10);
        send(s_SL, requisicao, sizeof(requisicao), 0);

        char resposta[BUFFER_SIZE];
        memset(resposta, 0, sizeof(resposta));
        recv(s_SL, resposta, sizeof(resposta), 0);

        if(((unsigned char)resposta[0] == ERROR) && (resposta[1] == '9')){
            close(s_SL);
            logexit("Sensor limit exceeded (SL)");
        }else if((unsigned char)resposta[0] == RES_CONNSEN){
            printf("SL New ID: %s\n", id);
        }
    }

    //conexao com ss
    if(porta == 61000){
        s_SS = socket(AF_INET, SOCK_STREAM, 0);
        if(s_SS == -1){
            logexit("socket SS");
        }

        struct sockaddr_in addr;
        char portac[10];
        sprintf(portac, "%d", porta);
        if(addrparse(ip, portac, &addr) != 0){
            logexit("addrparse");
        }

        if(connect(s_SS, (struct sockaddr *)&addr, sizeof(addr)) != 0){
            logexit("connect SS");
        }

        char requisicao[BUFFER_SIZE];
        memset(requisicao, 0, sizeof(requisicao));
        requisicao[0] = REQ_CONNSEN; 
        strncpy(&requisicao[1], id, 10);
        send(s_SS, requisicao, sizeof(requisicao), 0);

        char resposta[BUFFER_SIZE];
        memset(resposta, 0, sizeof(resposta));
        recv(s_SS, resposta, sizeof(resposta), 0);

        if(((unsigned char)resposta[0] == ERROR) && (resposta[1] == '9')){
            close(s_SS);
            logexit("Sensor limit exceeded (SS)");
        }else if((unsigned char)resposta[0] == RES_CONNSEN){
            printf("SS New ID: %s\n", id);
        }
    }
}

//comunicacao com o servidor
void comunicacao(char *input){
    int command_code = get_command_code(input);

    //status do sensor
    switch (command_code){
        case 1: {
            char req[BUFFER_SIZE];
            memset(req, 0, sizeof(req));
            req[0] = REQ_SENSSTATUS;
            strcpy(&req[1], id);  
            send(s_SS, req, sizeof(req), 0);

            char resposta[BUFFER_SIZE];
            memset(resposta, 0, sizeof(resposta));
            recv(s_SS, resposta, sizeof(resposta), 0);

            if((unsigned char)resposta[0] == RES_SENSSTATUS){
                char *alerta;
                switch(resposta[1]){
                    case '1': case '2': case '3': alerta = "Norte"; break;
                    case '4': case '5': alerta = "Sul"; break;
                    case '6': case '7': alerta = "Leste"; break;
                    case '8': case '9': case 'A': alerta = "Oeste"; break;
                    default: alerta = "Sem área definida"; break;
                }
                printf("Alert received from location: %s (%s)\n", &resposta[2], alerta);
            }
            else if((unsigned char)resposta[0] == ERROR){
                logexit("Sensor not found");
            }
            else if((unsigned char)resposta[0] == OK)
                printf("Status do sensor 0\n");
            break;
        }
        //lista de sensores na mesma localizacao
        case 2: {
            char req[BUFFER_SIZE];
            memset(req, 0, sizeof(req));
            req[0] = REQ_LOCLIST;
            strncpy(&req[1], id, 10);
            send(s_SL, req, sizeof(req), 0); 

            char resposta[BUFFER_SIZE];
            memset(resposta, 0, sizeof(resposta));
            recv(s_SL, resposta, sizeof(resposta), 0);

            if((unsigned char) resposta[0] == ERROR){
                logexit("Location not found");
            }
            else if((unsigned char) resposta[0] == RES_LOCLIST){
                printf("Sensors at Location %d : %s\n", resposta[1] ,&resposta[2]);
            }
            break;
        }

        //localizacao do sensor
        case 3: {
            char req[BUFFER_SIZE];
            memset(req, 0, sizeof(req));
            req[0] = REQ_SENSLOC;
            strncpy(&req[1], id, 10);
            send(s_SL, req, sizeof(req), 0);

            char resposta[BUFFER_SIZE];
            memset(resposta, 0, sizeof(resposta));
            recv(s_SL, resposta, sizeof(resposta), 0);

            if((unsigned char) resposta[0] == ERROR)
                printf("Sensor not found\n");
            else if ((unsigned char) resposta[0] == RES_SENSLOC)
                printf("Current sensor location: %s\n", &resposta[1]);
            break;
        }

        default:
            printf("Comando desconhecido.\n");
            break;
    }
}

int main(int argc, char **argv){
    srand(time(NULL));
    if(argc != 4){
        usage(argv[0]);
    }

    const char *ip = argv[1];
    char *porta_servidorSL = argv[2];
    char *porta_servidorSS = argv[3];

    gera_id(id);

    //portas definidas seguindo a documentação
    const int porta_SL = 60000;
    const int porta_SS = 61000;
    conexao(ip, porta_SL, id);
    conexao(ip, porta_SS, id);

    while(1){
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(s_SL, &read_fds);
        FD_SET(s_SS, &read_fds);

        max_fd = STDIN_FILENO > s_SL ? STDIN_FILENO : s_SL;
        if(s_SS > max_fd) max_fd = s_SS;

        if(select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0){
            logexit("select");
        }
        
        //kill recebido do terminal -> encerra as conexoes
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            char input[BUFFER_SIZE];
            memset(input, 0, sizeof(input));
            fgets(input, sizeof(input), stdin);
            if(strcmp(input, "kill\n") == 0){
                char kill_msg[BUFFER_SIZE];
                memset(kill_msg, 0, sizeof(kill_msg));
                kill_msg[0] = REQ_DISCSEN;
                strncpy(&kill_msg[1], id, 10);
                send(s_SS, kill_msg, sizeof(kill_msg), 0);
                send(s_SL, kill_msg, sizeof(kill_msg), 0);

                char resposta_SS[BUFFER_SIZE];
                char resposta_SL[BUFFER_SIZE];
                recv(s_SS, resposta_SS, sizeof(resposta_SS), 0);
                recv(s_SL, resposta_SL, sizeof(resposta_SL), 0);

                if(resposta_SS[0] == OK && resposta_SL[0] == OK){
                    printf("SS Successful disconnect\n");
                    printf("SL Successful disconnect\n");
                    close(s_SS);
                    close(s_SL);
                    exit(0);
                }
                else{
                    logexit("Sensor not found");
                }
            }else{ //troca de mensagens com o servidor

            comunicacao(input);
            continue;
            }
        }
    }

    return 0;
}
