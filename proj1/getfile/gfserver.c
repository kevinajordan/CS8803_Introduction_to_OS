#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "gfserver.h"

/*
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */

#define BUFSIZE 4096

struct gfserver_t {
    unsigned short port;
	char *requestedPath;
    int max_npending;
    void *handlerargument;
    ssize_t (*handler)(gfcontext_t *context, char *requestedPath, void *handlerargument);
    gfcontext_t *context;    
};

struct gfcontext_t {
    int listeningSocket;
    int connectionSocket;
    char* filepath;
};

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){
    
//    fprintf(stdout, "Starting header send.\n");
//    fflush(stdout);
    
    if (status == GF_ERROR) {
        ssize_t sendSize = write(ctx->connectionSocket, "GETFILE ERROR \r\n\r\n", strlen("GETFILE ERROR \r\n\r\n") + 1);
        fprintf(stdout, "GETFILE ERROR \r\n\r\n");
        fflush(stdout);
        gfs_abort(ctx);
        return sendSize;
    }
    
    else if (status == GF_FILE_NOT_FOUND) {
        ssize_t sendSize = write(ctx->connectionSocket, "GETFILE FILE_NOT_FOUND \r\n\r\n", strlen("GETFILE FILE_NOT_FOUND \r\n\r\n") + 1);
        fprintf(stdout, "GETFILE FILE_NOT_FOUND \r\n\r\n");
        fflush(stdout);
        gfs_abort(ctx);
        return sendSize;
    }
    else {
    fprintf(stdout, "Status is OK.\n");
    fflush(stdout);
    
    char filesizestring[500];
    sprintf(filesizestring, "%zu", file_len);
    
    char *header = (char *) malloc(17 + strlen(filesizestring));
    strcpy(header, "GETFILE OK ");
    strcat(header, filesizestring);
    strcat(header, "\r\n\r\n");
    ssize_t sendSize;
    sendSize = write(ctx->connectionSocket, header, strlen(header));

    if (header != NULL)
		free(header);
		
    return sendSize;
	}
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
    
   
    
    ssize_t write_len;
    write_len = write(ctx->connectionSocket, data, len);
//	fprintf(stdout, "file bytes sent to client: %zu.\n", write_len);
//    fflush(stdout);
    
    return write_len;
}

void gfs_abort(gfcontext_t *ctx){
    fprintf(stdout, "Abort Called.\n");
    fflush(stdout);
	
    
    close(ctx->connectionSocket);
}

gfserver_t* gfserver_create(){
    struct gfserver_t *gfs = malloc(sizeof(*gfs));
    return gfs;
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port){
    gfs->port = port;
}
void gfserver_set_maxpending(gfserver_t *gfs, int max_npending){
    gfs->max_npending = max_npending;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*)){
    gfs->handler = handler;
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg){
    gfs->handlerargument = arg;
}

void gfserver_serve(gfserver_t *gfs){
    int listeningSocket = 0;
    int connectionSocket = 0;
    int set_reuse_addr = 1;
    struct sockaddr_in client;
	struct sockaddr_in server;
    char *readData = "";
	socklen_t client_addr_len; 
    
    readData = (char*) malloc(BUFSIZE);
	
	listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (listeningSocket < 0 ){
			fprintf(stderr, "server failed to create the listening socket\n");
			return;
		}
        
        if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &set_reuse_addr, sizeof(set_reuse_addr)) != 0){
			fprintf(stderr, "server failed to set SO_REUSEADDR socket option (not fatal)\n");
		}
		
		
        
      
		bzero(&server, sizeof(server));
        
        server.sin_family = AF_INET;
		
        server.sin_addr.s_addr = htonl(INADDR_ANY);
        server.sin_port = htons(gfs->port);
        
        bind(listeningSocket, (struct sockaddr *) &server, sizeof(server));
        
        if (listen(listeningSocket, gfs->max_npending) < 0){
			fprintf(stderr, "server failed to listen\n");
			fflush(stderr);
			return;
		}else {
        fprintf(stdout, "server listening for a connection on port %d\n", gfs->port);
        fflush(stderr);
		}
    
	size_t readlen = 0;
	
    while(1) {
		
		char *completeheader = "";
		size_t bytestotal = 0;
		 size_t readlen = 0;

		client_addr_len = sizeof(client);
		connectionSocket = accept(listeningSocket, (struct sockaddr *) &client, &client_addr_len);
        if (0 > connectionSocket){
			fprintf(stderr, "server accept failed\n");
			fflush(stderr);
			return;
		}else{
			fprintf(stdout, "server accepted a client!\n");
        	fflush(stderr);
			
		}
        
       

         readlen = recv(connectionSocket, readData, BUFSIZE, 0);
		
         bytestotal += readlen;
        fprintf(stdout, "Received request from client: %s.\n", readData);
		fflush(stdout);
		 char *dataread = (char *) malloc(bytestotal + 1);
		strcpy(dataread, readData);
		completeheader = dataread;
        
        char *scheme = strtok(readData, " ");
        
        char *request = strtok(NULL, " ");
        
        char *filenamefromclient = strtok(NULL, " \r\n");
//        fprintf(stdout, "filename from client is:%s.\n", filenamefromclient); 
//		fflush(stdout);
		
		char * marker;
		if (filenamefromclient == NULL){
			marker = NULL;
		}
		else{
			marker = completeheader + strlen(scheme) + strlen(request) + strlen(filenamefromclient) + 2;		
//			fprintf(stdout, "Request scheme: %s. request: %s. filename:%s.  marker:%s.\n", scheme, request, filenamefromclient, marker);
//			fflush(stdout);
		}
		
        struct gfcontext_t *ctx = malloc(sizeof *ctx);
        ctx->filepath = filenamefromclient;
        ctx->listeningSocket = listeningSocket;
        ctx->connectionSocket = connectionSocket;
        
        
        if (scheme == NULL || request == NULL) {
            fprintf(stdout, "Send back FILE_NOT_FOUND due to incomplete header.\n");
            fflush(stdout);
 			gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	//		 close(ctx->connectionSocket);
        }
        else if (strcmp(scheme, "GETFILE") != 0 || strcmp(request, "GET") != 0) {
            fprintf(stdout, "Send back FILE_NOT_FOUND due to wrong schemes or methods.\n");
            fflush(stdout);
           	gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	//		 close(ctx->connectionSocket);
        }
		else if (readlen == 12 && filenamefromclient == NULL  || filenamefromclient != NULL && ( marker == "" || marker == NULL || (strlen(marker) == 1 && strcmp(marker, " ")!= 0 )|| (strlen(marker) == 4 && strcmp(marker, "\r\n\r\n") != 0 ) )  ) {
			 readlen = recv(connectionSocket, readData, BUFSIZE, 0);
			bytestotal += readlen;
//			fprintf(stdout, "Received another request from client: %s. request length = %d.\n", readData, strlen(readData));
			char *dataread2 = "";
			dataread2 = (char *) malloc(bytestotal + 1);
			strcpy(dataread2, dataread);

			strcat(dataread2, readData);
			completeheader = dataread2;
			
			char *scheme = strtok(dataread2, " ");
        
        	char *request = strtok(NULL, " ");
        
        	char *filenamefromclient = strtok(NULL, " \r\n");
			
			ssize_t bytestransferred = gfs->handler(ctx, filenamefromclient, gfs->handlerargument);
			if (bytestransferred < 0){
				gfs_sendheader(ctx, GF_ERROR, 0);
	//			close(ctx->connectionSocket);
			}
			
			if (dataread2 != NULL){
			free (dataread2);
			}
			

		}
        else {
//            fprintf(stdout, "calling handler.\n");
//            fflush(stdout);
            ssize_t bytestransferred = gfs->handler(ctx, filenamefromclient, gfs->handlerargument);
			if (bytestransferred < 0){
				gfs_sendheader(ctx, GF_ERROR, 0);
//				 close(ctx->connectionSocket);
			}
				
			fprintf(stdout, "Total file bytes transferred = %zu \n", bytestransferred);
			fflush(stdout);
//			close(ctx->connectionSocket);
//			fprintf(stdout, "Now client is disconnected.\n");
//			fflush(stdout);
        }
		if (dataread != NULL){
			free (dataread);
		}
		if (ctx != NULL)
			free(ctx);

    }
}