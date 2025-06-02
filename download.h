#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>


#define BUFFER_SIZE 1024


#define PORT_FTP 21

struct URL {
    char user[256];
    char password[256];
    char host[256];
    char path[256];
    char file_name[256];
    char host_name[256];    // host name gethostbyname()
    char ip[256];           // ip gethostbyname()
};





// Parse the URL
int parseURL(const char *link, struct URL *url);

// Establish connection to the FTP
int connection(char *ip, int port);

// Read Response
int readResponse(int sockfd, char *response, size_t response_size);

int parse_pasv_response(char *response, char *ip, int *port)

