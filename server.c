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

typedef struct{
    int sock;
    char user_id[11];
    int place_id;
    int cluster_id;
    int risk;

}sensor;

sensor *sensores[MAX_CLIENTES];

void usage(const char *progname){
    fprintf(stderr, "Uso: %s <ip> <porta_cliente> <porta_peer>\n", progname);
    exit(EXIT_FAILURE);
}

//inicializa o servidor
void cliente_servidor(const char *ip, const char *porta_cliente){
    //inicializa o endereço ipv4
    if(addrparse(ip, porta_cliente, &addr_client) != 0){
        logexit("addrparse");
    }

    //chama a função que inicializa o endereço do servidor
    if(server_socket_init(porta_cliente, &addr_client) != 0){
        logexit("server_socket_init");
    }  

    //cria o socket do servidor
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    if(bind(s, (struct sockaddr *)&addr_client, sizeof(addr_client)) != 0){
        logexit("bind");
    }

    //fica esperando a conexão dos clientes
    if(listen(s, MAX_CLIENTES) != 0){
        logexit("listen");
    }

    printf("Servidor ouvindo em %s:%s\n", ip, porta_cliente);

}


void peer2peer(const char *ip, const char *porta_peer){
    printf("debug 1\n");
    // Inicializa endereço do peer
    if (addrparse(ip, porta_peer, &peer) != 0){
        logexit("addrparse peer");
    }

    // Cria socket para escutar conexões
    listen_s = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_s == -1){
        logexit("socket listen");
    }

    // Cria socket para conectar
    socket_1 = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_1 == -1){
        logexit("socket connect");
    }

    int opt = 1;
    setsockopt(listen_s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  
    
    // Faz bind no listen_s
    if (bind(listen_s, (struct sockaddr *)&peer, sizeof(peer)) != 0) {
        logexit("bind listen");
    }

    // O listen só permite uma conexão simultânea
    if (listen(listen_s, 1) != 0){
        logexit("listen do peer");
    }

    // Tenta conectar ao peer remoto
    if(connect(socket_1, (struct sockaddr *)&peer, sizeof(peer)) == 0){
        printf("debug\n");
        // Conectado com sucesso — envia requisição
        char requisicao[BUFFER_SIZE];
        requisicao[0] = REQ_CONNPEER;

        gera_id(PidSi);
        printf("Peer %s connected\n", PidSi);
        strcpy(&requisicao[1], PidSi);
        send(socket_1, requisicao, sizeof(requisicao), 0);

        // Marca como cliente
        p2p_mode = 1;

    } else {
        printf("debug 2\n");
        // Não conseguiu conectar — escutará conexões
        printf("No peer found, starting to listen...\n");

        // Marca como servidor
        p2p_mode = 0;
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
    if(argc != 4){
        printf("oi");
        usage(argv[0]);
    }

    const char *ip = argv[1];
    const char *porta_peer = argv[2];
    const char *porta_cliente = argv[3];
    

    if(atoi(porta_cliente) <= 0 || atoi(porta_peer) <= 0){
        usage(argv[0]);
    }

    // printf("Servidor iniciado com IP %s\n", ip);
    // printf("Porta cliente: %s\n", porta_cliente);
    // printf("Porta peer: %s\n", porta_peer);

    //inicializa e conecta os peers
    peer2peer(ip, porta_peer);


    //inicializa o servidor do cliente
    // cliente_servidor(ip, porta_cliente);

    

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

        for(int i = 0; i < connected; i++){
            FD_SET(sensores[i]->sock, &read_fds);
        }

        if(select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1){
            logexit("select");
        }

        //kill
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            char input[BUFFER_SIZE];
            memset(input, 0, sizeof(input));
            fgets(input, sizeof(input), stdin);
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
