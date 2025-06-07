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
#define BUFFER_SIZE 501

#define REQ_CONNPEER 20
#define RES_CONNPEER 21
#define ERROR 255

int controle = 0;

void usage(const char *progname){
    fprintf(stderr, "Uso: %s <ip> <porta_cliente> <porta_peer>\n", progname);
    fprintf(stderr, "Exemplo: %s 127.0.0.1 6000 6100\n", progname);
    exit(EXIT_FAILURE);
}

void gera_id_peer(char id_peer[11]){
    for (int i = 0; i < 10; i++){
        id_peer[i] = '0' + (rand() % 10); 
    }
    id_peer[10] = '\0';
}

void cliente_servidor(const char *ip, const char *porta_cliente){
    struct sockaddr_in addr4; //IPV4

    //inicializa o ipv4
    if(addrparse(ip, porta_cliente, &addr4) != 0){
        logexit("addrparse");
    }

    //chama a função que inicializa o socket do servidor
    if(server_socket_init(porta_cliente, &addr4) != 0){
        logexit("server_socket_init");
    }  

    //cria o socket do servidor
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    if(bind(s, (struct sockaddr *)&addr4, sizeof(addr4)) != 0){
        logexit("bind");
    }

    //fica esperando a conexão dos clientes
    if(listen(s, MAX_CLIENTES) != 0){
        logexit("listen");
    }

    // printf("Servidor ouvindo em %s:%s\n", ip, porta_cliente);
    
    //troca de mensagens 
    while (1){
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
        // printf("Recebido %zd bytes do cliente: %s\n", count, buffer);

        send(socket_cliente, buffer, strlen(buffer), 0); 
        close(socket_cliente);
    }

    //fecha o socket do servidor
    close(s);
}


void peer2peer(const char *ip, const char *porta_peer){
    
    struct sockaddr_in peer;

    if(addrparse(ip, porta_peer, &peer) != 0){
        logexit("addrparse peer");
    }

    int socket_1 = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_1 == -1){
        logexit("socket peer");
    }  

    if(connect(socket_1, (struct sockaddr *)&peer, sizeof(peer)) != 0){
        printf("No peer found, starting to listen...\n");

        int listen_s = socket(AF_INET, SOCK_STREAM, 0); 
        if(listen_s == -1) logexit("socket listen");

        if(server_socket_init(porta_peer, &peer) != 0){
            logexit("server_socket_init peer");
        }

        if(bind(listen_s, (struct sockaddr *)&peer, sizeof(peer)) != 0){
            logexit("bind peer");
        }

        if(listen(listen_s, 2) != 0){
            logexit("listen peer");
        }

        struct sockaddr_in listenpeer_addr;
        socklen_t listenpeer_addr_len = sizeof(listenpeer_addr);

        while(1){
            socket_1 = accept(listen_s, (struct sockaddr *)&listenpeer_addr, &listenpeer_addr_len);
            if(socket_1 == -1) {
                logexit("accept peer");
                continue;
            }

            if(controle == 1){
                char error_msg[BUFFER_SIZE];
                memset(error_msg, 0, sizeof(error_msg));
                error_msg[0] = (unsigned char)ERROR;
                error_msg[1] = '1'; 
                send(socket_1, error_msg, sizeof(error_msg), 0);
                close(socket_1);  
                logexit("limite");
            }
            controle = 1;  
            break;         
        }
        
        char req_listenpeer[BUFFER_SIZE];
        memset(req_listenpeer, 0, sizeof(req_listenpeer));
        recv(socket_1, req_listenpeer, sizeof(req_listenpeer), 0);

        if(req_listenpeer[0] == REQ_CONNPEER){
                char PidSi[11];
                gera_id_peer(PidSi);
                printf("Peer %s connected\n", PidSi);

                char id_Si[BUFFER_SIZE] = {0};
                id_Si[0] = RES_CONNPEER;
                strncpy(&id_Si[1], PidSi, 10);
                send(socket_1, id_Si, sizeof(id_Si), 0);

                char id_Sj[BUFFER_SIZE] = {0};
                recv(socket_1, id_Sj, sizeof(id_Sj), 0);

                if(id_Sj[0] == RES_CONNPEER){
                    char PidSj[11] = {0};
                    strncpy(PidSj, &id_Sj[1], 10);
                    printf("New Peer ID: %s\n", PidSj);
                }else{
                    logexit("Error receiving peer ID");
                }
        }
        else{
            logexit("teste");
        }    
    }else{
        printf("Conexão estabelecida com o peer.\n");

        char requisicao[BUFFER_SIZE];
        requisicao[0] = REQ_CONNPEER;
        send(socket_1, requisicao, sizeof(requisicao), 0);

        memset(requisicao, 0, sizeof(requisicao));
        recv(socket_1, requisicao, sizeof(requisicao), 0);

        if((unsigned char)requisicao[0] == ERROR){
            logexit("Peer limit exceeded");
        }else{
            if(requisicao[0] == RES_CONNPEER){
            char PidSi[11] = {0};
            strncpy(PidSi, &requisicao[1], 10);
            printf("New Peer ID: %s\n", PidSi);

            char PidSj[11];
            gera_id_peer(PidSj);
            printf("Peer %s connected\n", PidSj);

            char id_Sj[BUFFER_SIZE];
            id_Sj[0] = RES_CONNPEER;
            strncpy(&id_Sj[1], PidSj, 10);
            send(socket_1, id_Sj, sizeof(id_Sj), 0);
            }   
        }
    }
    // close(socket_1);
    // controle = 0;    
}   



int main(int argc, char **argv){
    if (argc != 4){
        usage(argv[0]);
    }

    const char *ip = argv[1];
    const char *porta_peer = argv[2];
    const char *porta_cliente = argv[3];
    

    if (atoi(porta_cliente) <= 0 || atoi(porta_peer) <= 0){
        usage(argv[0]);
    }

    printf("Servidor iniciado com IP %s\n", ip);
    printf("Porta cliente: %s\n", porta_cliente);
    printf("Porta peer: %s\n", porta_peer);

    // cliente_servidor(ip, porta_cliente);

    peer2peer(ip, porta_peer);

    fd_set read_fds;
    int max_fd;

    // while(1){
    //     FD_ZERO(&read_fds);
    //     FD_SET(STDIN_FILENO, &read_fds); // Adiciona o stdin para leitura
    //     FD_SET(client_socket, &read_fds); // colocar todas as opcoes de socket onde o código está escutando

    //     //max_fd = //apenas ver qual socket é maior

    //     // Aguarda atividade nos sockets
    //     if(select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
    //         logexit("select");
    //     }

    //     // Verifica se há atividade no socket do servidor
    //     if(FD_ISSET(server_socket, &read_fds)){
    //         cliente_servidor(ip, porta_cliente);
    //     }
    // }

    return 0;
}
