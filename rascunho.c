if(FD_ISSET(s, &read_fds)){
            socklen_t addr_client_len = sizeof(addr_client);
            int client_sock = accept(s, (struct sockaddr *)&addr_client, &addr_client_len);
            if(client_sock == -1){
                logexit("accept");
            } else {
                if(connected >= MAX_CLIENTES){
                    char error_msg[BUFFER_SIZE] = {ERROR};
                    send(client_sock, error_msg, sizeof(error_msg), 0);
                    close(client_sock);
                    continue;
                }
                // Armazenar novo sensor, adicionar ao vetor `sensores`
            }
}


char req_sensor[BUFFER_SIZE];
        memset(req_sensor, 0, sizeof(req_sensor));
        recv(client_sock, req_sensor, sizeof(req_sensor), 0);

        if(req_sensor[0] == REQ_CONNPEER){
            gera_id(PidSi);
            printf("Sensor %s connected\n", PidSi);

            char id_Si[BUFFER_SIZE];
            id_Si[0] = RES_CONNPEER;
            strcpy(&id_Si[1], PidSi);
            send(client_sock, id_Si, sizeof(id_Si), 0);

            sensores[connected] = malloc(sizeof(sensor));
            sensores[connected]->sock = client_sock;
            strcpy(sensores[connected]->user_id, PidSi);
            sensores[connected]->place_id = -1; // Exemplo de inicialização
            sensores[connected]->cluster_id = -1; // Exemplo de inicialização
            sensores[connected]->risk = 0; // Exemplo de inicialização
            connected++;
        }
    }