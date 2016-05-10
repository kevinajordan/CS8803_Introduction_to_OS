#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#if 0
/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferserver [options]\n"                                                \
"options:\n"                                                                  \
"  -p                  Port (Default: 8888)\n"                                \
"  -f                  Filename (Default: bar.txt)\n"                         \
"  -h                  Show this help message\n"                              

int main(int argc, char **argv) {
  int option_char;
  int portno = 8888; /* port to listen on */
  char *filename = "bar.txt"; /* file to transfer */
  
  int maxnpending = 5;
  int socket_fd = 0;
  int client_socket_fd = 0;
  int set_reuse_addr = 1; // ON == 1
  
  
  struct sockaddr_in server;
  struct sockaddr_in client;
  struct hostent *client_host_info;
  char *client_host_ip;
  socklen_t client_addr_len; 
  char recvbuffer[BUFSIZE];
  

  // Parse and set command line arguments
  while ((option_char = getopt(argc, argv, "p:f:h")) != -1){
    switch (option_char) {
      case 'p': // listen-port
        portno = atoi(optarg);
        break;                                        
      case 'f': // listen-port
        filename = optarg;
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
  
   //create socket
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
	fprintf(stderr, "server failed to create the listening socket\n");
    exit(1);
  }
  
  
  // Set socket to use wildcards - i.e. 0.0.0.0:21 and 192.168.0.1:21
  // can be bound separately (helps to avoid conflicts) 
   if (0 != setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &set_reuse_addr, sizeof(set_reuse_addr))) {
	fprintf(stderr, "server failed to set SO_REUSEADDR socket option (not fatal)\n");
  } 
  
  // Configure server socket address structure (init to zero, IPv4,
  // network byte order for port and address)
  // Address uses local wildcard 0.0.0.0.0 (will connect to any local addr)
  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(portno);
  
  
  // Bind the socket
  if (bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "server failed to bind\n");
    exit(1);
  }
  
  // Listen on the socket for up to some maximum pending connections
  if (listen(socket_fd, maxnpending) < 0) {
    fprintf(stderr, "server failed to listen\n");
    exit(1);
  } else {
    fprintf(stdout, "server listening for a connection on port %d\n", portno);
  }
  
    // Get the size client's address structure
  
  
  while (1) {
  // Accept a new client
  client_addr_len = sizeof(client);
  client_socket_fd = accept(socket_fd, (struct sockaddr *)&client, &client_addr_len);
  if (client_socket_fd < 0) {
    fprintf(stderr, "server accept failed\n");
	exit(1);
  } else {
    fprintf(stdout, "server accepted a client!\n");    
  }
  
  // Determine who sent the echo so that we can respond
  client_host_info = gethostbyaddr((const char *)&client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), AF_INET);
  if (client_host_info == NULL) {
    fprintf(stderr, "server could not determine client host address\n");
  } 
  client_host_ip = inet_ntoa(client.sin_addr);
  if (client_host_ip == NULL) {
    fprintf(stderr, "server could not determine client host ip\n");
  }
  fprintf(stdout, "Server established connection with %s (%s)\n", client_host_info->h_name, client_host_ip);
  fprintf(stdout, "Server start receiving file %s from client and save as receivedfromclient.txt \n", filename);
  
  
  char* fr_name = "receivedfromclient.txt";
  FILE *fr = fopen(fr_name, "wb+");
  if(fr == NULL) {
    fprintf(stderr, "File %s Cannot be opened file on server.\n", fr_name);
    exit(1);
  }
  
  // server receives file from client
  bzero(recvbuffer, BUFSIZE);
  int fr_block_size = 0;

  while((fr_block_size = recv(client_socket_fd, recvbuffer, BUFSIZE, 0)) > 0) {
	fprintf(stdout, "%d bytes received \n",fr_block_size); 
	int write_size = fwrite(recvbuffer, sizeof(char), fr_block_size, fr);
	fprintf(stdout, "%d bytes written \n",write_size);
    if(write_size < fr_block_size) {
    fprintf(stderr, "File write failed on server.\n");
	exit(1);
    }
	bzero(recvbuffer, BUFSIZE);
  }
   
    if(fr_block_size < 0){
            if (errno == EAGAIN){
                fprintf(stdout, "recv() timed out.\n");
            }
            else{
            fprintf(stderr, "recv() failed due to errno = %d\n", errno);
  
            }
    }

  fprintf(stdout, "File received from client!\n");
  fclose(fr); 
  close(client_socket_fd);

  
  // accept connect again
  client_addr_len = sizeof(client);
  client_socket_fd = accept(socket_fd, (struct sockaddr *)&client, &client_addr_len);
  if (client_socket_fd < 0) {
    fprintf(stderr, "server accept failed\n");
	exit(1);
  } else {
    fprintf(stdout, "server accepted a client!\n");    
  }
  
  // Determine who sent the echo so that we can respond
  client_host_info = gethostbyaddr((const char *)&client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), AF_INET);
  if (client_host_info == NULL) {
    fprintf(stderr, "server could not determine client host address\n");
  } 
  client_host_ip = inet_ntoa(client.sin_addr);
  if (client_host_ip == NULL) {
    fprintf(stderr, "server could not determine client host ip\n");
  }
  fprintf(stdout, "Server established connection with %s (%s)\n", client_host_info->h_name, client_host_ip);
  
  //send file to client
  
  char sendbuffer[BUFSIZE];
  fprintf(stdout, "Server sending %s to the client...\n", filename);
  
         
  FILE *fs = fopen(filename, "ab+");
  if(fs == NULL) {
	fprintf(stderr, "File %s not found on server.\n", filename);
    exit(1);
  }

  bzero(sendbuffer, BUFSIZE); 
  int fs_block_size; 
 while((fs_block_size = fread(sendbuffer, sizeof(char), BUFSIZE, fs))>0){
	fprintf(stdout, "%d bytes sent\n",fs_block_size); 
	if(send(client_socket_fd, sendbuffer, fs_block_size, 0) < 0) {
		fprintf(stderr, "ERROR: Failed to send file %s.\n", filename);
        exit(1);
    }
    bzero(sendbuffer, BUFSIZE);
	if (fs_block_size == 0 || fs_block_size != BUFSIZE) {
		break;
	}
  }

  fprintf(stdout, "Ok File %s from server was Sent!\n", filename);
  fclose(fs);
  close(client_socket_fd);
  }
  close(socket_fd);
  return 0;

}