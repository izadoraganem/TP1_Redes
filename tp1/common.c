#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

// Mensagens de controle
#define REQ_CONNPEER 20
#define RES_CONNPEER 21
#define REQ_DISCPEER 22
#define REQ_CONNSEN 23
#define RES_CONNSEN 24
#define REQ_DISCSEN 25

// Mnesagens de dados
#define REQ_CHECKALERT 36
#define RES_CHECKALERT 37
#define REQ_SENSLOC 38
#define RES_SENSLOC 39
#define REQ_SENSSTATUS 40
#define RES_SENSSTATUS 41
#define REQ_LOCLIST 42
#define RES_LOCLIST 43

// Mensagens de confirmação
#define OK_DISCONNECT 1
#define OK_CREATE 2
#define OK_STATUS 3

// Payloads de confirmação
#define OK_01 "Sucessful disconnect\n"
#define OK_02 "Sucessful create\n"
#define OK_03 "Status do sensor 0\n"

// Mensagens de erro
#define ERROR 255
#define ERROR_PEER_LIMIT 1
#define ERROR_PEER_NOT_FOUND 2
#define ERROR_SENSOR_LIMIT 9
#define ERROR_SENSOR_NOT_FOUND 10
#define ERROR_LOCATION_NOT_FOUND 11

// Payloads erro
#define ERROR_01 "Peer limit exceeded\n"
#define ERROR_02 "Peer not found\n"
#define ERROR_09 "Sensor limit exceeded\n"
#define ERROR_10 "Sensor not found\n"
#define ERROR_11 "Location not found\n"

// Definições auxiliares
#define UNKNOWN "This message is not known\n"
#define MAX_SENSORS 15
#define MAX_P2P 1
#define MAX_MSG_SIZE 500

// Struct com as informações do sensor
typedef struct{
    int sock;         // socket
    char user_id[11]; // id
    bool risk;        // flag de risco
    int place_id;     // localização
} sensor;

// Struct com as informações da mensagem
typedef struct{
    int type;                   // tipo
    char payload[MAX_MSG_SIZE]; // payload
} message;

// Sockets
int p2p_socket, peer_socket, client_listen_socket, main_server_socket, ss_socket, sl_socket;

// Endereços
struct sockaddr_in p2p_addr, peer_addr, main_server_addr, client_addr;

// Controle da quantidade de sensores conectados
int connected_sensors = 0;

// Variáveis auxiliares
int server_role;
bool is_valid, at_risk;
char loc;
sensor* current_sensor;

// Portas dos servidores de status e localização
int ss_port, sl_port;

// Strings de identificação 
char my_id[11];
char peer_id[11];
char peer_sj[11];
char peer_si[11];

// Vetor de todos os sensores
sensor* all_sensors[MAX_SENSORS];

// Lida com erros no programa
void error(const char *msg){
    perror(msg);
    exit(1);
}

// Define papel do servidor
int define_server_role(int port){
    return port == 60000 ? 0 : 1; // 0: SL (Servidor de Localização), 1: SS (Servidor de Status)
}

// Define portas para conexão dos sensores -> 60000: SL (Servidor de Localização), 61000: SS (Servidor de Status)
void define_ports(int* sl_port, int* ss_port){
    if (*sl_port == 60000 && *ss_port == 61000) return;
    if (*sl_port == 61000 && *ss_port == 60000) {*sl_port = 60000; *ss_port = 61000;} 
    else {fprintf(stderr, "Invalid ports. Using default ports: 60000 for SL and 60001 for SS.\n"); *sl_port = 60000; *ss_port = 60001;}
}

// Mapeia localização para a região correspondente
char* map_cluster(int location_id) {
    switch(location_id) {
        case 1: case 2: case 3: return "Norte";
        case 4: case 5: return "Sul";
        case 6: case 7: return "Leste";
        case 8: case 9: case 10: return "Oeste";
        default: return "Desconhecido"; // Localização inválida (fora da zona ativa de cobertura)
    }
}

// Gera localização aleatória entre 1 e 10 associada a cada sensor
int generate_place_id() {
    return (rand() % 10) + 1;
}

// Gera risco de pane elétrica associado a cada sensor
int generate_risk() {
    int r = rand() % 100;  // Gera probabilidade de o sensor estar fora de risco
    if (r < 80) return (rand() % 2); // Sensor fora de risco com probabilidade de 80%
    return -1; // Sensor em risco com probabilidade de 20%
}

// Gera ID aleatório com 10 dígitos, para servidores ou sensores
void generate_id(char* id){
    for (int i = 0; i < 10; i++){
        id[i] = '0' + rand() % 10;
    }
    id[10] = '\0';
}

// Inicializa sensor com as informações comuns a ambos os servidores (socket, ID)
sensor* initialize_sensor(sensor* s, int socket_number, char* id) {
    s->sock = socket_number;
    strncpy(s->user_id, id, sizeof(s->user_id) - 1);
    s->user_id[sizeof(s->user_id) - 1] = '\0';

    return s;
}

// Verifica se um ID está na base de dados do servidor
bool check_sensor(char* id){
    for (int i = 0; i < connected_sensors; i++){
        if (strcmp(all_sensors[i]->user_id, id) == 0) return true;
    }
    return false;
}

// Busca sensor de um id
sensor find_sensor(char* id){
    for (int i = 0; i < connected_sensors; i++){
        if (strcmp(all_sensors[i]->user_id, id) == 0) return *all_sensors[i];
    }
    sensor not_found;
    not_found.sock = -1; // Sensor não encontrado
    return not_found;
}

// Busca todos os sensores presentes na localização loc_id
char* get_sensors_at_loc(int loc_id) {
    static char ids_list[MAX_MSG_SIZE] = "";
    ids_list[0] = '\0';
    int count = 0;
    
    for (int i = 0; i < connected_sensors; i++) {
        if (all_sensors[i]->place_id == loc_id) {
            if (count > 0) strcat(ids_list, ", ");
            strcat(ids_list, all_sensors[i]->user_id);
            count++;
        }
    }
    
    return ids_list;
}

// Envia mensagens para o socket destination
void sendMessage(int type, char* payload, int destination){
    message sent;
    memset(sent.payload, 0, MAX_MSG_SIZE);
    strncpy(sent.payload, payload, MAX_MSG_SIZE - 1);
    sent.payload[MAX_MSG_SIZE - 1] = '\0';
    sent.type = type;
    int n = write(destination, &sent, sizeof(sent));
    if (n < 0) error("ERROR writing to destination socket");
}

// Recebe mensagens do socket sender
message receiveMessage(int sender){
    message received;
    ssize_t n = read(sender, &received, sizeof(received));
    if (n < 0) error ("ERROR reading from socket");
    return received;    
} 