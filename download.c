#include "download.h"

// Parse the URL
int parseURL(const char *link, struct URL *url){
    if (strncmp(link, "ftp://", 6) != 0) {
        fprintf(stderr, "Error: Invalid URL. Must start with 'ftp://'\n");
        return -1;
    }

    link += 6;

    memset(url, 0, sizeof(struct URL));

    const char *at_sign = strchr(link, '@');
    const char *first_slash = strchr(link, '/');
    if (at_sign) {
        sscanf(link, "%255[^:]:%255[^@]", url->user, url->password);
        link = at_sign + 1; 
    } else {
        strcpy(url->user, "anonymous");
        strcpy(url->password, "anonymous");
    }

    if (first_slash) {
        sscanf(link, "%255[^/]/%s", url->host, url->path);
    } else {
        strcpy(url->host, link);
        strcpy(url->path, "");
    }

    const char *last_slash = strrchr(url->path, '/');
    if (last_slash) {
        strcpy(url->file_name, last_slash + 1);
    } else {
        strcpy(url->file_name, url->path); 

    struct hostent *h;
    if ((h = gethostbyname(url->host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    strcpy(url->host_name, h->h_name);
    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}

// Establish connection to the FTP
int connection(char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;


    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

// Read Response
int readResponse(int socketfd, char *response, size_t response_size) {
    size_t totalBytes = 0;
    int rcode = -1;
    int multiline = 0;
    char answer[BUFFER_SIZE];

    memset(response, 0, response_size);

    while(1) {
        ssize_t bytes;
        if ((bytes = read(socketfd, answer, BUFFER_SIZE)) < 0) {
            perror("read()\n");
            return -1;
        }

        answer[bytes] = '\0';

        if (totalBytes + bytes >= response_size) {
            return -1;
        }

        strcat(response, answer);
        totalBytes += bytes;

        if (rcode == -1) {
            sscanf(response, "%d", &rcode);
        
            const char *line_end = strchr(response, '\n');
            if (line_end && *(line_end - 1) == '-') {
                multiline = 1;
            }
        }

        if (multiline) {
            char *last_line = strrchr(response, '\n');
            if (last_line) {
                int last_code;
                if (sscanf(last_line + 1, "%d ", &last_code) == 1 && last_code == rcode) 
                    break; 
            }
        } else {
            break; 
        }
    }

    return rcode;
}

int parse_pasv_response(char *response, char *ip, int *port) {
    int h1, h2, h3, h4, p1, p2;
    char *start = strchr(response, '(');
    if (!start) return -1;

    sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = (p1 * 256) + p2;

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    // Parse the FTP URL
    struct URL url;
    if(parseURL(argv[1], &url) != 0) {
        perror("parseURL()\n");
        exit(-1);
    }

    printf("Connecting to:\n host: %s\n user: %s\n password: %s\n file path: %s\n file name: %s\n", url.host, url.user, url.password, url.path, url.file_name);
    printf(" host_name: %s\n Ip: %s\n", url.host_name, url.ip);

    // Set up control connection
    int control_sockfd = connection(url.ip, PORT_FTP);
    if(control_sockfd < 0) {
        perror("connection()\n");
        exit(-1);
    }
    

    // Read welcome message
    char response[BUFFER_SIZE];
    if (readResponse(control_sockfd, response, BUFFER_SIZE) < 0) {
        perror("readResponse()\n");
        close(control_sockfd);
        exit(-1);
    }

    printf("\n%s\n", response);

    // Authenticate
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "USER %s\r\n", url.user);
    if (write(control_sockfd, command, strlen(command)) < 0) {
        perror("write()");
        exit(-1);
    }
    if (readResponse(control_sockfd, response, BUFFER_SIZE) < 0) {
        perror("readResponse()\n");
        close(control_sockfd);
        exit(-1);
    }
    printf("%s\n", response);

    snprintf(command, sizeof(command), "PASS %s\r\n", url.password);
    if (write(control_sockfd, command, strlen(command)) < 0) {
        perror("write()");
        exit(-1);
    }
    if (readResponse(control_sockfd, response, BUFFER_SIZE) < 0) {
        perror("readResponse()\n");
        close(control_sockfd);
        exit(-1);
    }
    printf("%s\n", response);

    // Enter passive mode
    if (write(control_sockfd, "PASV\r\n", strlen("PASV\r\n")) < 0) {
        perror("write()");
        exit(-1);
    }
    if (readResponse(control_sockfd, response, BUFFER_SIZE) < 0) {
        perror("readResponse()\n");
        close(control_sockfd);
        exit(-1);
    }
    printf("%s\n", response);

    // Parse passive mode response
    char data_ip[INET_ADDRSTRLEN];
    int data_port;
    if (parse_pasv_response(response, data_ip, &data_port) != 0) {
        perror("Failed to enter Passive mode.\n");
        exit(-1);
    }
    printf("Passive Mode IP: %s, Port: %d\n", data_ip, data_port);


    // Connect to the data socket
    int data_sockfd = connection(data_ip, data_port);
    if(data_sockfd < 0) {
        perror("connection()\n");
        exit(-1);
    }

    // Request file download
    snprintf(command, sizeof(command), "RETR %s\r\n", url.path);
    if (write(control_sockfd, command, strlen(command)) < 0) {
        perror("write()");
        exit(-1);
    }
    if (readResponse(control_sockfd, response, BUFFER_SIZE) < 0) {
        perror("readResponse()\n");
        close(control_sockfd);
        exit(-1);
    }
    printf("%s\n", response);

    // Receive file data
    FILE *file = fopen(url.file_name, "wb");
    if (!file) {
        perror("fopen()");
        exit(-1);
    }

    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = read(data_sockfd, buffer, BUFFER_SIZE)) > 0) {
        fwrite(buffer, 1, bytes, file);
    }
    fclose(file);
    printf("File downloaded successfully: %s\n", url.path);

    // Clean up
    close(data_sockfd);
    close(control_sockfd);

    return 0;
}
