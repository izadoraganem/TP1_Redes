#include "common.c"

// Conecta o sensor ao server na porta server_port
int connect_to_server(int server_port, char* client_id, char* ip) {
    // Cria o socket do servidor
    printf("IM TRYING\n");
    main_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_server_socket < 0) error("ERROR opening client socket");

    // Configura o endereço do servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        close(main_server_socket);
        error("Invalid target IP address");
    }

    // Tenta conectar ao servidor
    if (connect(main_server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(main_server_socket);
        error("ERROR connecting to server");
    }

    // Resquisita conexão do sensor client_id
    sendMessage(REQ_CONNSEN, client_id, main_server_socket);
    return main_server_socket;
}

// Gerencia criação do sensor e a conexão a cada um dos servidores
void connect_client(int server_1st, int server_2nd, char* ip) {
    generate_id(my_id);

    // Conexão ao SS (Servidor de Status)
    ss_socket = connect_to_server(server_1st, my_id, ip);
    message ss_msg = receiveMessage(ss_socket);
    if (ss_msg.type == RES_CONNSEN) printf("SS New ID: %s\n", ss_msg.payload);
    else if (ss_msg.type == ERROR_SENSOR_LIMIT) {printf("%s", ss_msg.payload); close(ss_socket); exit(0);}
    else {printf(UNKNOWN); exit(1);}

    // Conexão ao SL (Servidor de Localização)
    sl_socket = connect_to_server(server_2nd, my_id, ip);
    message sl_msg = receiveMessage(sl_socket);
    if (sl_msg.type == RES_CONNSEN) printf("SL New ID: %s\n", sl_msg.payload);
    else if (sl_msg.type == ERROR_SENSOR_LIMIT) {printf("%s", sl_msg.payload); close(sl_socket); exit(0);}
    else {printf(UNKNOWN); exit(1);}

    printf(OK_02);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <first_server> <second_server>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];       // Endereço IP
    sl_port = atoi(argv[2]);  // Porta para conexão com SL
    ss_port = atoi(argv[3]);  // Porta para conexão com SS

    // Cria seed para geração de números aleatórios
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned int seed = tv.tv_sec ^ tv.tv_usec ^ getpid();
    srand(seed);

    // Define portas para SS e SL
    define_ports(&sl_port, &ss_port);
    // Conexta sensor aos servidores
    connect_client(ss_port, sl_port, ip);

    fd_set read_fds;
    int max_fd;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(ss_socket, &read_fds);
        FD_SET(sl_socket, &read_fds);

        // Seleciona a entrada
        max_fd = STDIN_FILENO > ss_socket ? STDIN_FILENO : ss_socket;
        if (sl_socket > max_fd) max_fd = sl_socket;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
            error("ERROR on select");

        // Gerencia entrada do usuário
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char buffer[256];
            memset(buffer, 0, 256); // Limpa o buffer

            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                printf("Closing server due to stdin EOF.\n");
                break;
            }
            buffer[strcspn(buffer, "\n")] = 0; 

            if (strcmp(buffer, "kill") == 0) {
                sendMessage(REQ_DISCSEN, my_id, ss_socket);
                sendMessage(REQ_DISCSEN, my_id, sl_socket);

                message ss_msg = receiveMessage(ss_socket);
                message sl_msg = receiveMessage(sl_socket);

                if (ss_msg.type == OK_DISCONNECT && sl_msg.type == OK_DISCONNECT) {
                    printf("SS Successful disconnect\nSL Successful disconnect\n");
                    close(ss_socket);
                    close(sl_socket);
                    break;
                    exit(0);
                } else {
                    printf(ERROR_10);
                    exit(1);
                } 
            } 

            // Gerencia comando "Check failure" e verifica risco do sensor
            else if (strcmp(buffer, "Check failure") == 0) {
                sendMessage(REQ_SENSSTATUS, my_id, ss_socket);
                message ss_msg = receiveMessage(ss_socket);
                if (ss_msg.type == RES_SENSSTATUS) printf("Alert received from location: %s (%s)\n", ss_msg.payload, map_cluster(atoi(ss_msg.payload)));
                else if (ss_msg.type == ERROR_SENSOR_NOT_FOUND) printf(ERROR_10);
                else if (ss_msg.type == OK_STATUS) printf(OK_03);
                else printf(UNKNOWN);
                continue;
            }

            // Gerencia comando "locate" e verifica localização do sensor
            else if (strncmp(buffer, "locate", 6) == 0) {
                char* sensor_id = buffer + 7; 
                sendMessage(REQ_SENSLOC, sensor_id, sl_socket);
                message sl_msg = receiveMessage(sl_socket);
                if (sl_msg.type == ERROR_SENSOR_NOT_FOUND) printf(ERROR_10);
                else if (sl_msg.type == RES_SENSLOC) printf("Current sensor location: %s\n", sl_msg.payload);
                else printf(UNKNOWN);
                continue;
            }

            // Gerencia comando "diagnose" e verifica sensores em uma localização
            else if (strncmp(buffer, "diagnose", 8) == 0) {
                char* loc_id = buffer + 9; 
                sendMessage(REQ_LOCLIST, loc_id, sl_socket);
                message diagnose_msg = receiveMessage(sl_socket);
                if (diagnose_msg.type == ERROR_LOCATION_NOT_FOUND) printf(ERROR_11);
                else if (diagnose_msg.type == RES_LOCLIST) printf("%s\n", diagnose_msg.payload);
                else printf(UNKNOWN);
                continue;
            }

            else printf(UNKNOWN);
        }

        // Verifica se a conexão com SS foi desfeita por inatividade do servidor 
        if (FD_ISSET(ss_socket, &read_fds)) {

            int n = read(ss_socket, &read_fds, sizeof(read_fds));
            if (n <= 0) {
                printf("SS server was closed. Exiting...\n");
                close(ss_socket);
                exit(0);
            }

        }

        // Verifica se a conexão com SL foi desfeita por inatividade do servidor 
        if (FD_ISSET(sl_socket, &read_fds)) {
            
            int n = read(sl_socket, &read_fds, sizeof(read_fds));
            if (n <= 0) {
                printf("SL server was closed. Exiting...\n");
                close(sl_socket);
                exit(0);
            }
        }
    }

    // Garante o fechamento de todos os sockets abertos ao longo da execução
    close(ss_socket);
    close(sl_socket);
    close(main_server_socket);

    return 0;
}