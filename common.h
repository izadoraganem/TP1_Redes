#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void logexit(const char *msg);

void usage(const char *progname);

int addrparse(const char *ip_str, const char *port_str, struct sockaddr_in *addr);

int addrtostr(const struct sockaddr *addr, char *str, size_t strsize) ;

int server_socket_init(const char *portstr, struct sockaddr_in *addr4);

void gera_id(char id[11]);

int get_command_code(const char* buffer);