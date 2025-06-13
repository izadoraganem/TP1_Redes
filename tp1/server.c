#include "common.c"

// Inicializa escuta por clientes
void start_server(int client_port) {
    // Cria o socket do servidor
    client_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_listen_socket < 0) error("ERROR opening client socket");

    // Configura o endereço do servidor
    memset(&main_server_addr, 0, sizeof(main_server_addr));
    main_server_addr.sin_family = AF_INET;
    main_server_addr.sin_addr.s_addr = INADDR_ANY;
    main_server_addr.sin_port = htons(client_port);

    // Bind no endereço do servidor
    if (bind(client_listen_socket, (struct sockaddr *)&main_server_addr, sizeof(main_server_addr)) < 0)
        error("ERROR binding client socket");

    // Escuta por conexões de clientes
    listen(client_listen_socket, MAX_SENSORS);
}

// Inicializa servidor P2P
void start_p2p_server(int p2p_port, char* ip) {
    // Cria o socket P2P
    p2p_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (p2p_socket < 0) error("ERROR opening P2P socket");

    int opt = 1;
    if (setsockopt(p2p_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        error("ERROR setting SO_REUSEADDR");

    #ifdef SO_REUSEPORT
    // Permite reuso da porta
    if (setsockopt(p2p_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        error("ERROR setting SO_REUSEPORT");
    #else
    fprintf(stderr, "SO_REUSEPORT not supported on this system.\n");
    #endif

    // Configura o endereço P2P
    memset(&p2p_addr, 0, sizeof(p2p_addr));
    p2p_addr.sin_family = AF_INET;
    p2p_addr.sin_port = htons(p2p_port);
    if (inet_pton(AF_INET, ip, &p2p_addr.sin_addr) <= 0) {
        close(p2p_socket);
        error("P2P: Invalid peer target IP address");
    }
    
    // Bind no endereço P2P
    if (bind(p2p_socket, (struct sockaddr *)&p2p_addr, sizeof(p2p_addr)) < 0)
        error("ERROR binding my P2P socket");

    // Escuta por conexões de peers
    listen(p2p_socket, MAX_P2P);
}

// Inicializa peer
void start_peer_p2p(int peer_port, char* ip){
    // Cria o socket peer
    peer_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_socket < 0) error("ERROR opening P2P socket");

    // Configura o endereço peer
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    if (inet_pton(AF_INET, ip, &peer_addr.sin_addr) <= 0) {
        close(peer_socket);
        error("P2P: Invalid peer target IP address");
    }
}

// Busca conexão com peer
void connect_p2p(int p2p_port, char* ip){
    int temp_socket = connect(peer_socket, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
    start_p2p_server(p2p_port, ip);

    // O servidor Sj ainda não está aberto para conexão com Si
    if (temp_socket != 0) {
        printf("No peer found, starting to listen...\n");
        
        fd_set con_read_fds;
        int con_max_fd;
        while(1){
            FD_ZERO(&con_read_fds);
            FD_SET(STDIN_FILENO, &con_read_fds);
            FD_SET(p2p_socket, &con_read_fds);
            
            con_max_fd = STDIN_FILENO > p2p_socket ? STDIN_FILENO : p2p_socket;

            if (select(con_max_fd + 1, &con_read_fds, NULL, NULL, NULL) < 0)
                error("ERROR on select");

            // Gerencia entrada do usuário
            if (FD_ISSET(STDIN_FILENO, &con_read_fds)) {
                char buffer[256];
                bzero(buffer, 256);
                if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                    printf("Closing server due to stdin EOF.\n");
                    break;
                }
                buffer[strcspn(buffer, "\n")] = 0;
                if (strcmp(buffer, "kill") == 0) {
                    close(p2p_socket);
                    exit(0);
                }
                else printf(UNKNOWN);
            }

            // Recebe conexão do peer
            if (FD_ISSET(p2p_socket, &con_read_fds)) {
                socklen_t addr_len = sizeof(p2p_addr);
                peer_socket = accept(p2p_socket, (struct sockaddr *)&p2p_addr, &addr_len);
                if (peer_socket < 0) error("ERROR accepting P2P connection");

                message msg = receiveMessage(peer_socket);
                printf("New Peer ID: %s\n", msg.payload);
                strcpy(my_id, msg.payload);

                generate_id(peer_sj);
                strcpy(peer_id, peer_sj);
                printf("Peer %s connected\n", peer_sj);   
                sendMessage(RES_CONNPEER, peer_sj, peer_socket);
                break;
            }
        }
    }
    
    // O servidor Sj aceita a conexão do servidor Si
    else {
        generate_id(peer_si);
        strcpy(peer_id, peer_si);
        printf("Peer %s connected\n", peer_si);
        
        // Solicita conexão ao peer
        sendMessage(REQ_CONNPEER, peer_si, peer_socket);

        fd_set fds;
        struct timeval timeout;
        FD_ZERO(&fds);
        FD_SET(peer_socket, &fds);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int ret = select(peer_socket + 1, &fds, NULL, NULL, &timeout);

        if (ret > 0 && FD_ISSET(peer_socket, &fds)){
            message msg = receiveMessage(peer_socket);
            printf("New Peer ID: %s\n", msg.payload);
            strcpy(my_id, msg.payload);
        } else {
            printf(ERROR_01);
            close(peer_socket);
            exit(0);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <peer_port> <client_port>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];               // Endereço IP
    int p2p_port = atoi(argv[2]);     // Porta para conexão P2P
    int client_port = atoi(argv[3]);  // Porta para escutar clientes 

    // Cria seed para geração de números aleatórios
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned int seed = tv.tv_sec ^ tv.tv_usec ^ getpid();
    srand(seed);

    // Inicializa peer
    start_peer_p2p(p2p_port, ip);
    // Busca conexão com peer
    connect_p2p(p2p_port, ip);

    // Inicializa escuta por clientes
    start_server(client_port);

    // Define papel do servidor 
    server_role = define_server_role(client_port);

    fd_set read_fds;
    int max_fd;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(peer_socket, &read_fds);
        FD_SET(client_listen_socket, &read_fds);

        for (int i = 0; i < connected_sensors; i++) {
            FD_SET(all_sensors[i]->sock, &read_fds);
        }

        // Seleciona a entrada
        max_fd = peer_socket > client_listen_socket ? peer_socket : client_listen_socket;
        if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        for (int i = 0; i < connected_sensors; i++) {
            if (all_sensors[i]->sock > max_fd) max_fd = all_sensors[i]->sock;
        }

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
                sendMessage(REQ_DISCPEER, my_id, peer_socket);

                message msg = receiveMessage(peer_socket);

                if (msg.type == ERROR){
                    printf(ERROR_02);
                    exit(0);
                } else if (msg.type == OK_DISCONNECT){
                    close(p2p_socket);
                    break;
                    exit(0);
                } else printf(UNKNOWN); continue;
            }
            else printf(UNKNOWN); // Mensagem no buffer não conhecida
        }

        // Gerencia mensagens recebidas do peer
        if (FD_ISSET(peer_socket, &read_fds)) {
            message peer_msg = receiveMessage(peer_socket);

            switch (peer_msg.type){

                case REQ_DISCPEER:
                    if (strcmp(peer_msg.payload, peer_id) == 0) {
                        strcpy(peer_id, ""); // Remove o peer dos peers conectados
                        sendMessage(OK_DISCONNECT, OK_01, peer_socket); 
                        printf("Peer %s disconnected\n", peer_msg.payload);
                        connect_p2p(p2p_port, ip);
                    }
                    else sendMessage(ERROR_PEER_NOT_FOUND, ERROR_02, peer_socket);
                    break;

                case REQ_CHECKALERT:
                    printf("REQ_CHECKALERT %s", peer_msg.payload);
                    bool is_valid = check_sensor(peer_msg.payload);
                    if (is_valid) {
                        loc = find_sensor(peer_msg.payload).place_id + '0';
                        sendMessage(RES_CHECKALERT, &loc, peer_socket);
                    } else sendMessage(ERROR_SENSOR_NOT_FOUND, ERROR_10, peer_socket);
                    break;

                case ERROR_SENSOR_NOT_FOUND:
                    printf(ERROR_10);
                    break;

                case ERROR_PEER_NOT_FOUND:
                    printf(ERROR_02);
                    break;
                    
                default:
                    sendMessage(ERROR, UNKNOWN, p2p_socket);
                    break;
            }
        }

        // Gerencia novas conexões de sensores
        if (FD_ISSET(client_listen_socket, &read_fds)) {
            
            socklen_t addr_len = sizeof(main_server_addr);
            int temp = accept(client_listen_socket, (struct sockaddr *)&main_server_addr, &addr_len);
            if (temp < 0) error("ERROR accepting P2P connection");

            // Verifica se foi atingida a quantidade máxima de sensores
            else if (connected_sensors >= MAX_SENSORS) {
                sendMessage(ERROR_SENSOR_LIMIT, ERROR_09, temp);
                close(temp);
                continue;
            }

            message client_msg = receiveMessage(temp);

            switch (client_msg.type){
                case REQ_CONNSEN:
                    all_sensors[connected_sensors] = (sensor *)malloc(sizeof(sensor));
                    current_sensor = initialize_sensor(all_sensors[connected_sensors], temp, client_msg.payload);

                    // Servidor SL inicializa localização
                    if (server_role == 0) {
                        current_sensor->place_id = generate_place_id();
                        printf("Client %s added (Loc %d)\n", all_sensors[connected_sensors]->user_id, all_sensors[connected_sensors]->place_id);
                    }

                    // Servidor SS inicializa risco 
                    else {
                        current_sensor->risk = generate_risk();
                        printf("Client %s added %d\n", all_sensors[connected_sensors]->user_id, all_sensors[connected_sensors]->risk);
                    }

                    sendMessage(RES_CONNSEN, all_sensors[connected_sensors]->user_id, temp);
                    connected_sensors++;
                    break;

                default:
                    printf("Unknown message type %d received from client\n", client_msg.type);
                    sendMessage(ERROR, UNKNOWN, temp);
                    close(temp);
                    break;
            }
        }

        // Gerencia mensagens de sensores conectados
        for (int i = 0; i < connected_sensors; i++) {
            if (FD_ISSET(all_sensors[i]->sock, &read_fds)) {
                message sensor_msg = receiveMessage(all_sensors[i]->sock);

                switch (sensor_msg.type) {
                    case REQ_DISCSEN:
                        // Verifica se o ID está na base de dados do servidor 
                        is_valid = check_sensor(sensor_msg.payload);

                        if (is_valid) {
                            sendMessage(OK_DISCONNECT, OK_01, all_sensors[i]->sock);

                            // Servidor SL informa localização do sensor removido
                            if (server_role == 0) printf("Client %s removed (Loc %d)\n", all_sensors[i]->user_id, all_sensors[i]->place_id);

                            // Servidor SS informa risco do sensor removido
                            else printf("Client %s removed %d\n", all_sensors[i]->user_id, all_sensors[i]->risk);

                            close(all_sensors[i]->sock);
                            all_sensors[i] = all_sensors[connected_sensors - 1];
                            connected_sensors--;
                        }
                        else sendMessage(ERROR_SENSOR_NOT_FOUND, ERROR_10, all_sensors[i]->sock);
                        break;

                    case REQ_SENSSTATUS:
                        // Verifica se o sensor está em risco
                        at_risk = all_sensors[i]->risk;
                        if (at_risk) {
                            sendMessage(REQ_CHECKALERT, all_sensors[i]->user_id, peer_socket);
                            message msg = receiveMessage(peer_socket);

                            if (msg.type == RES_CHECKALERT) sendMessage(RES_SENSSTATUS, msg.payload, all_sensors[i]->sock);
                            else if (msg.type == ERROR_SENSOR_NOT_FOUND) sendMessage(ERROR_SENSOR_NOT_FOUND, ERROR_10, all_sensors[i]->sock);
                            else sendMessage(ERROR, UNKNOWN, all_sensors[i]->sock);

                        } else sendMessage(OK_STATUS, OK_03, all_sensors[i]->sock);
                        break;

                    case REQ_SENSLOC:
                        printf("REQ_SENSLOC %s\n", sensor_msg.payload);

                        // Verifica se o ID está na base de dados do servidor 
                        is_valid = check_sensor(sensor_msg.payload);

                        if (is_valid){
                            loc = find_sensor(sensor_msg.payload).place_id + '0';
                            sendMessage(RES_SENSLOC, &loc, all_sensors[i]->sock);

                        } else sendMessage(ERROR_SENSOR_NOT_FOUND, ERROR_10, all_sensors[i]->sock);
                        break;

                    case REQ_LOCLIST:
                        printf("REQ_LOCLIST %s\n", sensor_msg.payload);
                        char* sensors_at_loc = get_sensors_at_loc(atoi(sensor_msg.payload));
                        
                        // Verifica se existe algum sensor na localização especificada 
                        if (strcmp(sensors_at_loc, "") == 0) sendMessage(ERROR_LOCATION_NOT_FOUND, ERROR_11, all_sensors[i]->sock);
                        else {
                            char final_msg[MAX_MSG_SIZE] = "";
                            strcat(final_msg, "Sensors at location ");
                            strcat(final_msg, sensor_msg.payload);
                            strcat(final_msg, ": ");
                            strcat(final_msg, sensors_at_loc);
                            sendMessage(RES_LOCLIST, final_msg, all_sensors[i]->sock);
                        }
                        break;

                    default:
                        sendMessage(ERROR, UNKNOWN, all_sensors[i]->sock);
                        break;
                }
            }
        }
    }

    // Garante o fechamento de todos os sockets abertos ao longo da execução
    close(p2p_socket);
    close(peer_socket);
    close(client_listen_socket);

    return 0;
}