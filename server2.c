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
#define REQ_CONNSEN 23
#define RES_CONNSEN 24
#define REQ_DISCSEN 25


#define REQ_CHECKALERT 36
#define RES_CHECKALERT 37
#define REQ_SENSLOC 38
#define RES_SENSLOC 39
#define REQ_SENSSTATUS 40
#define RES_SENSSTATUS 41
#define REQ_LOCLIST 42
#define RES_LOCLIST 43
#define ERROR 255
#define OK 0

int clientes = 0;
int peers_connected = 0;
int s;
int socket_1;
int listen_s;
char PidSi[11];
char PidSj[11];
int tipo_servidor;
int client_socket;

typedef struct{
    int socket;         
    int risco;        
    char id_cliente[11];
    int local;

} sensor;

sensor* sensores[MAX_CLIENTES];

// struct sockaddr_in peer;
// socklen_t peer_len = sizeof(peer);

int verify(const char *id){
    for(int i = 0; i < clientes; i++){
        if(strcmp(sensores[i]->id_cliente, id) == 0){
            return 1; 
        }
    }
    return 0; 
}

void requisicoes(char *requisicao_cliente, int i){

    if ((unsigned char)requisicao_cliente[0] == REQ_SENSSTATUS) {
        int status = -1;
        printf("REQ_SENSSTATUS %s\n", &requisicao_cliente[1]);

        if (verify(&requisicao_cliente[1])){
            for (int j = 0; j < clientes; j++) {
                if (sensores[j] && strcmp(sensores[j]->id_cliente, &requisicao_cliente[1]) == 0) {
                    status = sensores[j]->risco;
                    break;
                }
            }
        } 

        switch (status) {
            char resposta[BUFFER_SIZE];

            case 0:
                memset(resposta, 0, sizeof(resposta));
                resposta[0] = OK;
                strcpy(&resposta[1], &requisicao_cliente[1]);
                send(sensores[i]->socket, resposta, sizeof(resposta), 0);
                break;

            case 1:
                memset(resposta, 0, sizeof(resposta));
                resposta[0] = REQ_CHECKALERT;
                strcpy(&resposta[1], &requisicao_cliente[1]);
                send(socket_1, resposta, sizeof(resposta), 0);
                printf("Sending REQ_CHECKALERT %s to SL\n", &requisicao_cliente[1]);

                memset(resposta, 0, sizeof(resposta));
                int r = recv(s, resposta, sizeof(resposta), 0);

                //envia para o cliente a resposta
                if ((unsigned char)resposta[0] == RES_CHECKALERT){
                    send(sensores[i]->socket, resposta, sizeof(resposta), 0);
                } else if ((unsigned char)resposta[0] == ERROR){
                    send(sensores[i]->socket, resposta, sizeof(resposta), 0);
                }
                break;

            case -1:
                printf("[DEBUG] Cliente não encontrado na lista de sensores\n");
                memset(resposta, 0, sizeof(resposta));
                resposta[0] = (unsigned char)ERROR;
                strcpy(&resposta[1], "10");
                send(sensores[i]->socket, resposta, sizeof(resposta), 0);
                break;
        }

    } else if ((unsigned char)requisicao_cliente[0] == REQ_LOCLIST) {
        int temp_loc = sensores[i]->local;
        
        printf("REQ_LOCLIST %d\n", temp_loc);

       char list[BUFFER_SIZE] = {0};

        for (int j = 0; j < clientes; j++) {
            if (sensores[j] && sensores[j]->local == temp_loc) {
                if (strlen(list) > 0) strcat(list, ", ");
                strcat(list, sensores[j]->id_cliente); // id_cliente já é string
            }
        }

        char resposta[BUFFER_SIZE];
        memset(resposta, 0, sizeof(resposta));

        if (strlen(list) == 0) {
            resposta[0] = (unsigned char)ERROR;
            strcpy(&resposta[1], "11");
        } else {
            resposta[0] = RES_LOCLIST;
            resposta[1] = temp_loc;  // você deve usar isso **antes** da lista
            strcpy(&resposta[2], list); // a lista entra a partir do índice 2
        }


        send(sensores[i]->socket, resposta, sizeof(resposta), 0);

    } else if ((unsigned char)requisicao_cliente[0] == REQ_SENSLOC) {
        printf("REQ_SENSLOC %s\n", &requisicao_cliente[1]);
        int found = verify(&requisicao_cliente[1]);

        char resposta[BUFFER_SIZE];
        memset(resposta, 0, sizeof(resposta));

        if (found == 1) {
            resposta[0] = RES_SENSLOC;
            char buffer[16];
            sprintf(buffer, "%d", sensores[i]->local);
            strcpy(&resposta[1], buffer);
        } else {
            resposta[0] = (unsigned char)ERROR;
            strcpy(&resposta[1], "10");
        }

        send(sensores[i]->socket, resposta, sizeof(resposta), 0);

    } else if ((unsigned char)requisicao_cliente[0] == REQ_DISCSEN) {
        int found = verify(&requisicao_cliente[1]);
        char resposta[BUFFER_SIZE];
        memset(resposta, 0, sizeof(resposta));

        if (found == 0) {
            resposta[0] = (unsigned char)ERROR;
            resposta[1] = 10;
        } else {
            resposta[0] = OK;
            
            printf("Client %s removed\n", sensores[i]->id_cliente);
        

            close(sensores[i]->socket);
            sensores[i]->socket = -1;
            clientes--;
            sensores[i] = sensores[clientes];
        }

        send(sensores[i]->socket, resposta, sizeof(resposta), 0);
    } else {
        printf("[DEBUG] Código de requisição desconhecido: %d\n", (unsigned char)requisicao_cliente[0]);
    }
}
void cliente_servidor(const char *ip, const char *porta_cliente){
    struct sockaddr_in addr4; // IPV4
    fd_set read_fds;
    int max_fd;

    // inicializa o ipv4
    if(addrparse(ip, porta_cliente, &addr4) != 0){
        logexit("addrparse");
    }

    // cria o socket do servidor
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    if(bind(s, (struct sockaddr *)&addr4, sizeof(addr4)) != 0){
        logexit("bind");
    }

    // fica esperando conexões
    if(listen(s, MAX_CLIENTES) != 0){
        logexit("listen");
    }

    while(1){
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(s, &read_fds);

        max_fd = s > STDIN_FILENO ? s : STDIN_FILENO;

        // adiciona os sensores conectados
        for(int i = 0; i < clientes; i++){
            FD_SET(sensores[i]->socket, &read_fds);
            if(sensores[i]->socket > max_fd){
                max_fd = sensores[i]->socket;
            }
        }

        // espera por atividade
        if(select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0){
            logexit("select");
        }

        // comando kill no terminal
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = 0;

            if(strcmp(buffer, "kill") == 0){
                close(s);
                for(int i = 0; i < clientes; i++) {
                    close(sensores[i]->socket);
                    free(sensores[i]);
                }
                exit(0);
            }
        }

        // nova conexão
        if(FD_ISSET(s, &read_fds)){
            struct sockaddr_storage client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(s, (struct sockaddr*)&client_addr, &client_len);

            if(client_socket == -1){
                logexit("accept sensor");
            }

            if(clientes >= MAX_CLIENTES){
                char error_msg[BUFFER_SIZE] = {0};
                error_msg[0] = (unsigned char)ERROR;
                error_msg[1] = '9'; 
                send(client_socket, error_msg, sizeof(error_msg), 0);
                close(client_socket);
                continue;
            }

            char requisicao[BUFFER_SIZE] = {0};
            recv(client_socket, requisicao, sizeof(requisicao), 0);

            if(requisicao[0] == REQ_CONNSEN){
                sensor *novo = malloc(sizeof(sensor));
                if (!novo) {
                    perror("malloc");
                    close(client_socket);
                    continue;
                }

                novo->socket = client_socket;
                strcpy(novo->id_cliente, &requisicao[1]);

                if (tipo_servidor == 60000){
                    novo->local = rand() % 10 + 1;
                    printf("Client %s added (%d)\n", novo->id_cliente, novo->local);
                } else {
                    novo->risco = rand() % 2;
                    printf("Client %s added (%d)\n", novo->id_cliente, novo->risco);
                }

                char resposta[BUFFER_SIZE] = {0};
                resposta[0] = RES_CONNSEN;
                strcpy(&resposta[1], novo->id_cliente);
                send(client_socket, resposta, sizeof(resposta), 0);

                sensores[clientes++] = novo;
            }
        }

        // dados de sensores conectados
        for(int i = 0; i < clientes; i++){
            if(FD_ISSET(sensores[i]->socket, &read_fds)){
                char requisicao_cliente[BUFFER_SIZE] = {0};
                int bytes = recv(sensores[i]->socket, requisicao_cliente, sizeof(requisicao_cliente), 0);

                if(bytes <= 0){
                    printf("Sensor %s desconectado.\n", sensores[i]->id_cliente);
                    close(sensores[i]->socket);
                    free(sensores[i]);
                    sensores[i] = sensores[--clientes]; // substitui pelo último
                    i--;
                    continue;
                }
                requisicoes(requisicao_cliente, i);
            }
        }
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
                    printf("Succesful disconnect");
                    printf("Peer %s disconnected\n", PidSi);
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
                strcpy(PidSj, &req_listenpeer[1]);
                printf("Peer %s connected\n", PidSj);

                gera_id(PidSi);
                printf("New Peer ID: %s\n", PidSi);
                char id_Si[BUFFER_SIZE];
                id_Si[0] = RES_CONNPEER;
                strcpy(&id_Si[1], PidSi);
                send(conn_fd, id_Si, sizeof(id_Si), 0);
                break;
            }
        }
    }   
    }else{
        if(peers_connected){
            logexit("Peer limit exceeded");
        }else{
            gera_id(PidSj);
            
            char requisicao[BUFFER_SIZE];
            requisicao[0] = REQ_CONNPEER;
            strcpy(&requisicao[1], PidSj);
            send(socket_1, requisicao, sizeof(requisicao), 0);

            memset(requisicao, 0, sizeof(requisicao));
            recv(socket_1, requisicao, sizeof(requisicao), 0);
            if(requisicao[0] == RES_CONNPEER){
            printf("New Peer ID: %s\n", PidSj);
            strcpy(PidSi, &requisicao[1]);
            printf("Peer %s connected\n", PidSi);
            } 
        }          
    }   
}   

void comunicacao_peers(char *requisicao_peer){
    if((unsigned char)requisicao_peer[0] == ERROR){
        if((unsigned char)requisicao_peer[1] == 'A'){
            printf("Sensor not found\n");
            exit(0);
        }else if((unsigned char)requisicao_peer[1] == '2'){
            printf("Peer not found\n");
            exit(0);
        }

    }else if((unsigned char)requisicao_peer[0] == REQ_CHECKALERT){
        char temp_id[16];
        char buffer[16];
        strcpy(temp_id, &requisicao_peer[1]);
        if(verify(temp_id)){
            char resposta[BUFFER_SIZE];
            memset(resposta, 0, sizeof(resposta));
            resposta[0] = RES_CHECKALERT;
            for(int i = 0; i < MAX_CLIENTES; i++){
                if (strcmp(sensores[i]->id_cliente, temp_id) == 0)
                    sprintf(buffer, "%d", sensores[i]->local);
                    strcpy(&resposta[1], buffer);
            }
            send(socket_1, resposta, sizeof(resposta), 0);
        }else{
            char resposta[BUFFER_SIZE];
            memset(resposta, 0, sizeof(resposta));
            resposta[0] = (unsigned char)ERROR;
            resposta[1] = 'A'; // Sensor not found
            send(socket_1, resposta, sizeof(resposta), 0);
        }
    }else if((unsigned char) requisicao_peer[0] == REQ_DISCPEER){
        if(strcmp(&requisicao_peer[1], PidSi) == 0){
            printf("Peer %s disconnected\n", PidSi);
            close(socket_1);
            exit(0);
        }else if(strcmp(&requisicao_peer[1], PidSj) == 0){
            printf("Peer %s disconnected\n", PidSj);
            close(socket_1);
            exit(0);
        }else{
            char resposta[BUFFER_SIZE];
            memset(resposta, 0, sizeof(resposta));
            resposta[0] = (unsigned char)ERROR;
            resposta[1] = '2';
            send(socket_1, resposta, sizeof(resposta), 0);
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


    tipo_servidor = atoi(porta_cliente);
    // printf("tipo servidor: %d\n", tipo_servidor);

    peer2peer(ip, porta_peer);
    cliente_servidor(ip, porta_cliente);


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

        for(int i = 0; i < clientes; i++){
            FD_SET(sensores[i]->socket, &read_fds);
        }

        if(select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1){
            logexit("select");
        }

        //kill
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            char input[BUFFER_SIZE];
            memset(input, 0, sizeof(input));
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = 0;

            if(strcmp(input, "kill") == 0){
                printf("Succesful disconnect");
                printf("Peer %s disconnected\n", PidSj);
                close(socket_1);
                printf("Peer %s disconnected\n", PidSi);
                close(listen_s);
                return 0;
            }
        }

        if(FD_ISSET(socket_1, &read_fds)){
            char requisicao_peer[BUFFER_SIZE];
            memset(requisicao_peer, 0, sizeof(requisicao_peer));
            recv(socket_1, requisicao_peer, sizeof(requisicao_peer), 0);

            comunicacao_peers(requisicao_peer);
        }

        

    }
    return 0;
}