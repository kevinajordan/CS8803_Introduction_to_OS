#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>

/* Be prepared accept a response of this length */
#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoclient [options]\n"                                                    \
"options:\n"                                                                  \
"  -s                  Server (Default: localhost)\n"                         \
"  -p                  Port (Default: 8888)\n"                                \
"  -m                  Message to send to server (Default: \"Hello World!\"\n"\
"  -h                  Show this help message\n"                              

/* Main ========================================================= */
int main(int argc, char **argv) {
	int option_char = 0;
	char *hostname = "localhost";
	unsigned short portno = 8888;
	char *message = "Hello World!";
	int sockfd = 0;
	struct sockaddr_in serv_addr;
	struct hostent *he = gethostbyname(hostname);
	unsigned long server_addr_nbo = *(unsigned long *)(he->h_addr_list[0]);
	char buffer[BUFSIZE];
	
    

	// Parse and set command line arguments
	while ((option_char = getopt(argc, argv, "s:p:m:h")) != -1) {
		switch (option_char) {
			case 's': // server
				hostname = optarg;
				break; 
			case 'p': // listen-port
				portno = atoi(optarg);
				break;                                        
			case 'm': // server
				message = optarg;
				break;    
			case 'h': // help
				fprintf(stdout, "%s", USAGE);
				exit(0);
				break;       
			default:
				fprintf(stderr, "%s", USAGE);
				exit(1);
		}
	}

	/* Socket Code Here */
	
	 // Create socket (IPv4, stream-based, protocol likely set to TCP)
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) { 
        fprintf(stderr, "client failed to create socket\n");
		exit(1);
	}
	
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = server_addr_nbo;
	
	//Connect socket to server
	 if (connect(sockfd,&serv_addr,sizeof(serv_addr)) < 0) {
		fprintf(stderr, "client failed to connect to %s:%d!\n", hostname, portno);
		close(sockfd);
    exit(1);
  } else {
    fprintf(stdout, "client connected to to %s:%d!\n", hostname, portno);
  }
	//send message
	if (send(sockfd, message, strlen(message), 0) < 0) {
    fprintf(stderr, "client failed to send echo message");
    close(sockfd);
    exit(1);
  }
  
	//echo response from server
    bzero(buffer, BUFSIZE);
	
	if(read(sockfd, buffer, BUFSIZE) < 0) {
    fprintf(stderr, "client could not read response from server\n");
    close(sockfd);
    exit(1);
  } else {
    fprintf(stdout, "echo from server: %s\n", buffer);
  }
  
  // Close the socket and return the response length (in bytes)
  close(sockfd);
  return 0; 

}
