#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "common.h"
#include <time.h>

#define MAX_CLIENTES 15
#define BUFFER_SIZE 501

//mensagens de controle
#define REQ_CONNPEER 20
#define RES_CONNPEER 21
#define REQ_DISCPEER 22
#define REQ_DISCSEN 25


#define ERROR 255
#define OK 0

int connected = 0;
int peers_connected = 0;
int s;
int socket_1;
int listen_s;
char PidSi[11];
char PidSj[11];
struct sockaddr_in addr_client; 
int p2p_mode = 0; 
int peer_accepted = 0; 
struct sockaddr_in peer;
socklen_t peer_len = sizeof(peer);

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
}


void peer2peer(const char *ip, const char *porta_peer){
    struct sockaddr_in peer;

    int socket_1 = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_1 == -1){
        logexit("socket peer");
    }  

    if(server_socket_init(porta_peer, &peer) != 0){
        logexit("server_socket_init peer");
    }

    if(connect(socket_1, (struct sockaddr *)&peer, sizeof(peer)) != 0){
        fd_set read_fds;
        int max_fd;

        printf("No peer found, starting to listen...\n");

        int listen_s = socket(AF_INET, SOCK_STREAM, 0); 
        if(listen_s == -1){
            logexit("socket listen");
        }

        int opt = 1;
        if(setsockopt(listen_s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        logexit("setsockopt");

        //configura endereço do peer
        if(addrparse(ip, porta_peer, &peer) != 0){
            logexit("server_socket_init peer");
        }

        if(bind(listen_s, (struct sockaddr *)&peer, sizeof(peer)) != 0){
            logexit("bind peer");
        }

        socklen_t peer_len = sizeof(peer);

        if(listen(listen_s, 2) != 0){
            logexit("listen peer");
        }
        while(1){
        //     struct sockaddr_in listenpeer_addr;
        //     socklen_t listenpeer_addr_len = sizeof(listenpeer_addr);
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(listen_s, &read_fds);

            max_fd = listen_s > STDIN_FILENO ? listen_s : STDIN_FILENO;

            if(select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0){
                logexit("select");
            }

            //pode dar kill enquanto escuta

            if(FD_ISSET(STDIN_FILENO, &read_fds)){
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, sizeof(buffer));
                fgets(buffer, sizeof(buffer), stdin);

                buffer[strcspn(buffer, "\n")] = 0;
                if(strcmp(buffer, "kill") == 0){
                    printf("Killing server...\n");
                    close(socket_1);
                    exit(0);
                }
            }

            if(FD_ISSET(listen_s, &read_fds)){
                int conn_fd;
                while(1){
                    conn_fd = accept(listen_s, (struct sockaddr *)&peer, &peer_len);

                if(conn_fd == -1){
                    continue; 
                }
                    
                if(peers_connected == 1){
                    char error_msg[BUFFER_SIZE];
                    memset(error_msg, 0, sizeof(error_msg));
                    error_msg[0] = (unsigned char)ERROR;
                    error_msg[1] = '1'; 
                    send(conn_fd, error_msg, sizeof(error_msg), 0);
                    close(conn_fd);  
                    logexit("limite");
                }
                peers_connected = 1;  
                break;         
            }
                
            char req_listenpeer[BUFFER_SIZE];
            memset(req_listenpeer, 0, sizeof(req_listenpeer));
            recv(conn_fd, req_listenpeer, sizeof(req_listenpeer), 0);

            if(req_listenpeer[0] == REQ_CONNPEER){
                strcpy(PidSi, &req_listenpeer[1]);
                printf("New Peer ID: %s\n", PidSi);

                //gera id do sj e envia para ele
                gera_id_peer(PidSj);
                char id_Sj[BUFFER_SIZE] ;
                id_Sj[0] = RES_CONNPEER;
                strcpy(&id_Sj[1], PidSj);
                send(conn_fd, id_Sj, sizeof(id_Sj), 0);
                break;
            }
        }
    }   
    }else{
        fd_set fds;
        struct timeval timeout;
        FD_ZERO(&fds);
        FD_SET(socket_1, &fds);

        printf("Conexão estabelecida com o peer.\n");
        
        gera_id_peer(PidSi);
        printf("Peer %s connected\n", PidSi);

        char requisicao[BUFFER_SIZE];
        requisicao[0] = REQ_CONNPEER;
        strcpy(&requisicao[1], PidSi);
        send(socket_1, requisicao, sizeof(requisicao), 0);

        //recebe resposta com id do sj
        memset(requisicao, 0, sizeof(requisicao));
        recv(socket_1, requisicao, sizeof(requisicao), 0);

        if((unsigned char)requisicao[0] == ERROR){
            logexit("Peer limit exceeded");
        }else{
            if(requisicao[0] == RES_CONNPEER){
            strcpy(PidSj, &requisicao[1]);
            printf("New Peer ID: %s\n", PidSj);
            }   
        }
    }  
}   

void kill_peer2peer(){
    if (socket_1 != -1){

    char kill_msg[BUFFER_SIZE];
    kill_msg[0] = (unsigned char) REQ_DISCPEER;
    strcpy(&kill_msg[1], PidSi);  

    if(send(socket_1, kill_msg, sizeof(kill_msg), 0) == -1){
        logexit("send kill_msg");
    }

    char resposta_kill[BUFFER_SIZE];
    memset(resposta_kill, 0, sizeof(resposta_kill));

    if(recv(socket_1, resposta_kill, sizeof(resposta_kill), 0) == -1){
        logexit("recv kill_msg");
    }

    if((unsigned char)resposta_kill[0] == ERROR){
        printf("Peer not found\n");
    }
    else if((unsigned char)resposta_kill[0] == OK){
        printf("Peer %s disconnected\n", PidSj);
    }

        close(socket_1);
        socket_1 = -1;

        if(listen_s != -1){
            close(listen_s);  
            listen_s = -1;
        }
    }
}


int main(int argc, char **argv){
    srand(time(NULL));
    if (argc != 4){
        usage(argv[0]);
    }

    const char *ip = argv[1];
    const char *porta_peer = argv[2];
    const char *porta_cliente = argv[3];
    

    if (atoi(porta_cliente) <= 0 || atoi(porta_peer) <= 0){
        usage(argv[0]);
    }

    // printf("Servidor iniciado com IP %s\n", ip);
    // printf("Porta cliente: %s\n", porta_cliente);
    // printf("Porta peer: %s\n", porta_peer);

    // cliente_servidor(ip, porta_cliente);

    peer2peer(ip, porta_peer);

    fd_set read_fds;
    int max_fd;

    while(1){
        // colocar todas as opcoes de socket onde o código está escutando
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); 
        FD_SET(s, &read_fds); //cliente servidor s
        FD_SET(socket_1, &read_fds); //peer2peer socket_1
        FD_SET(listen_s, &read_fds); //peer2peer listen_s

        max_fd = STDIN_FILENO > s ? STDIN_FILENO : s;
        max_fd = max_fd > socket_1 ? max_fd : socket_1;
        max_fd = max_fd > listen_s ? max_fd : listen_s;

        // for(int i = 0; i < connected; i++){
        //     FD_SET(sensores[i]->sock, &read_fds);
        // }

        if(select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1){
            logexit("select");
        }

        //kill
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            char input[BUFFER_SIZE];
            memset(input, 0, sizeof(input));
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = 0;

            if(strcmp(input, "kill\n") == 0){
                kill_peer2peer();
            }
        }

        if(FD_ISSET(listen_s, &read_fds) && p2p_mode == 0){
            int new_peer = accept(listen_s, (struct sockaddr *)&peer, &peer_len);
            if(new_peer == -1){
                logexit("accept peer");
            }
        
        socket_1 = new_peer;

        char req_peer[BUFFER_SIZE];
        recv(socket_1, req_peer, sizeof(req_peer), 0);

        strcpy(PidSi, &req_peer[1]);
        peers_connected++;

        gera_id(PidSj);
        printf("Peer %s connected\n", PidSj);

        char id_Sj[BUFFER_SIZE];
        id_Sj[0] = RES_CONNPEER;
        strcpy(&id_Sj[1], PidSj);
        send(socket_1, id_Sj, sizeof(id_Sj), 0);

        p2p_mode = 0; // servidor aceitando conexão
    }


        if(FD_ISSET(socket_1, &read_fds) && p2p_mode == 1){
        // Recebe RES_CONNPEER
        char resposta[BUFFER_SIZE];
        if(recv(socket_1, resposta, sizeof(resposta), 0) <= 0){
            logexit("recv peer");
        }

        if(peers_connected > 1){
            printf("New Peer ID: %s\n", &resposta[1]);
        } else {
            printf("Peer limit exceeded\n");
        }
    }

    }
    close(s);
    close(socket_1);
    close(listen_s);
    return 0;
}