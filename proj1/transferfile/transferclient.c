#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferclient [options]\n"                                                \
"options:\n"                                                                  \
"  -s                  Server (Default: localhost)\n"                         \
"  -p                  Port (Default: 8888)\n"                                \
"  -o                  Output file (Default foo.txt)\n"                       \
"  -h                  Show this help message\n"                              

/* Main ========================================================= */
int main(int argc, char **argv) {
	int option_char = 0;
	char *hostname = "localhost";
	unsigned short portno = 8888;
	char *filename = "foo.txt";
	int sockfd = 0;
	struct sockaddr_in serv_addr;
	struct hostent *he = gethostbyname(hostname);
	unsigned long server_addr_nbo = *(unsigned long *)(he->h_addr_list[0]);
	char recvbuffer[BUFSIZE];
	

	// Parse and set command line arguments
	while ((option_char = getopt(argc, argv, "s:p:o:h")) != -1) {
		switch (option_char) {
			case 's': // server
				hostname = optarg;
				break; 
			case 'p': // listen-port
				portno = atoi(optarg);
				break;                                        
			case 'o': // filename
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
	if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
		fprintf(stderr, "client failed to connect to %s:%d!\n", hostname, portno);
		close(sockfd);
		exit(1);
	} else {
    fprintf(stdout, "client connected to to %s:%d!\n", hostname, portno);
	}
	
	// send file to server
	char sendbuffer[BUFSIZE];
	fprintf(stdout, "Client start sending file %s to server\n", filename);
	FILE *fs = fopen(filename, "ab+");
	
	if(fs == NULL)
        {
            fprintf(stderr, "File %s not found.\n", filename);
			close(sockfd);
            exit(1);
        }
	
	bzero(sendbuffer, BUFSIZE);
	int fs_block_size = 0; 
	
    while((fs_block_size = fread(sendbuffer, sizeof(char), BUFSIZE, fs)) > 0) {
		fprintf(stdout, "%d bytes sent\n",fs_block_size); 
		if(send(sockfd, sendbuffer, fs_block_size, 0) < 0)
            {
                fprintf(stderr, "ERROR: Failed to send file %s.\n", filename);
				close(sockfd);
				exit(1);
            }
            bzero(sendbuffer, BUFSIZE);
		if (fs_block_size == 0 || fs_block_size != BUFSIZE) {

			break;
		}
	}
	fclose(fs);
	bzero(sendbuffer, BUFSIZE);
	fprintf(stdout, "Ok File %s from Client was Sent!\n", filename);
	close(sockfd);
	fprintf(stdout, "disconnected after send\n");
	
	//connect again
	
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
	
	if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
		fprintf(stderr, "client failed to connect to %s:%d!\n", hostname, portno);
		close(sockfd);
		exit(1);
	} else {
    fprintf(stdout, "client connected again to to %s:%d!\n", hostname, portno);
	}
	
	
	 /* Receive File from Server */
    fprintf(stdout, "Client receiveing file from Server and saving it as receivedfromserver.txt...\n");	
    char* fr_name = "receivedfromserver.txt";
	
	
    FILE *fr = fopen(fr_name, "wb+");
    if(fr == NULL) {
        fprintf(stderr, "File %s Cannot be opened.\n", fr_name);
		close(sockfd);
		exit(1);
	}
    else
    {
        bzero(recvbuffer, BUFSIZE); 
        int fr_block_size = 0;

        while((fr_block_size = recv(sockfd, recvbuffer, BUFSIZE, 0)) > 0) {
			fprintf(stdout, "%d bytes received \n",fr_block_size); 
            int write_size = fwrite(recvbuffer, sizeof(char), fr_block_size, fr);
			
            if(write_size < fr_block_size)
            {
                fprintf(stderr, "file write failed");
				close(sockfd);
				exit(1);
				
            }
			
            bzero(recvbuffer, BUFSIZE);
		}
            

		
        
        if(fr_block_size < 0)
        {
            if (errno == EAGAIN){
                fprintf(stdout, "recv() timed out.\n");
            }
            else{
            fprintf(stderr, "recv() failed due to errno = %d\n", errno);
			close(sockfd);
            exit(1);
            }
        }

        fprintf(stdout, "Ok received from server!\n");
        fclose(fr);
		bzero(recvbuffer, BUFSIZE);
		
    }
	
    close (sockfd);
    fprintf(stdout, "Disconnected now.\n");
    return 0;
}