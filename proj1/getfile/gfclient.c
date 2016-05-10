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

#include "gfclient.h"

#define BUFSIZE 4096

struct gfcrequest_t  {
    char *server;
    char *path;
    unsigned short port;
    gfstatus_t status;
    char *statusText;
    size_t bytesOfFileReceived;
    size_t filelength;
    char *filelengthstring;
    void *writerargument;
    void *headerargument;
    void (*writerfunc)(void *, size_t, void *);
    void (*headerfunc)(void *, size_t, void *);
    void *fileContent;
};

gfcrequest_t *gfc_create(){
    struct gfcrequest_t *gfr = malloc(sizeof *gfr);
    return gfr;
}

void gfc_set_server(gfcrequest_t *gfr, char* server){
    gfr->server = server;
}

void gfc_set_path(gfcrequest_t *gfr, char* path){
    gfr->path = path;
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port){
    gfr->port = port;
}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)){
    gfr->headerfunc = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg){
    gfr->headerargument = headerarg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)){
    gfr->writerfunc = writefunc;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg){
    gfr->writerargument = writearg;
}

int gfc_perform(gfcrequest_t *gfr){
    int clientSocket = 0;
    struct sockaddr_in serverSocketAddress;
    char receivedData[BUFSIZE];
    int set_reuse_addr = 1;
    int headerComplete = 0;
    
	// creat client socket
    if (0 > (clientSocket = socket(AF_INET, SOCK_STREAM, 0))){
		fprintf(stderr, "client failed to create socket\n");
		return -1;
	}
    
    setsockopt(clientSocket, SOL_SOCKET, SO_REUSEADDR, &set_reuse_addr, sizeof(set_reuse_addr));
    
    struct hostent *he = gethostbyname(gfr->server);
    unsigned long server_addr_nbo = *(unsigned long *)(he->h_addr_list[0]);
    
    bzero(&serverSocketAddress, sizeof(serverSocketAddress));
    serverSocketAddress.sin_family = AF_INET;
    serverSocketAddress.sin_port = htons(gfr->port);
    serverSocketAddress.sin_addr.s_addr = server_addr_nbo;
    
    if (0 > connect(clientSocket, (struct sockaddr *)&serverSocketAddress, sizeof(serverSocketAddress))){
		fprintf(stderr, "client failed to connect to %s:%d!\n", gfr->server, gfr->port);
		close(clientSocket);
		return -1;
	} else {
        fprintf(stdout, "client connected to to %s:%d!\n", gfr->server, gfr->port);
	}
	
	bzero(receivedData, BUFSIZE);
    
	
	// make header file 
	// 22 = string length of "GETFILE GET " + " \r\n\r\n" + \0
    char * message = (char *) malloc(18 + strlen(gfr->path) );
    strcpy(message, "GETFILE GET ");
    strcat(message, gfr->path);
    strcat(message, "\r\n\r\n");
    
    send(clientSocket, message, strlen(message), 0);
    
//    fprintf(stdout, "Header Sent: %s.\n", message);
 //   fflush(stdout);
    
    char buffer[BUFSIZE];
    size_t bytesTotal = 0;
    size_t bytesTotalOfFile = 0;
	
	if (message != NULL)
		free(message);
    
    while (1) {
        size_t bytesRead = recv(clientSocket, buffer, BUFSIZE, 0);
//        fprintf(stdout, "bytesread from server is %d \n", bytesRead);
        
        if (bytesRead < 0) {
            return -1;
        }
        
        bytesTotal = bytesTotal + bytesRead;        

        if (bytesRead == 0) {
            fprintf(stdout, "Connection stopped. Bytes total: %zu.\n", bytesTotal);
            fflush(stdout);
            close(clientSocket);
            if (bytesTotalOfFile < gfr->filelength){
				gfc_cleanup(gfr);
                return -1;
			}
			gfc_cleanup(gfr);
            return 0;
        }
       
        //bytesread > 0;
        if  (bytesRead > 0) {  
            if (headerComplete != 1) {           
 //               fprintf(stdout, "Parse header\n");
 //               fflush(stdout);        
                char * dataForAnalysis = (char *) malloc(bytesRead + 1);
                memcpy(dataForAnalysis, buffer, bytesRead);
                
                if (bytesRead <= 7){
					gfr->statusText = "INVALID";
					gfr->status = gfc_get_status(gfr);
					gfr->bytesOfFileReceived = 0;
					if (gfr->headerargument != NULL) {
                        gfr->headerfunc("INVALID", strlen("INVALID"), gfr->headerargument);
                    }
                    free(dataForAnalysis);
					gfc_cleanup(gfr);
					return -1;
				}		
            
                if (bytesRead > 7) {
                    char *scheme = strtok(dataForAnalysis, " ");
                    if (strcmp(scheme, "GETFILE") != 0) {
                        gfr->statusText = "INVALID";
                        gfr->status = gfc_get_status(gfr);
                        gfr->bytesOfFileReceived = 0;
                        free(dataForAnalysis);
						gfc_cleanup(gfr);
                        return -1;
                    }
					
					if (bytesRead <= 10){
						gfr->statusText = "INVALID";
                        gfr->status = gfc_get_status(gfr);
                        gfr->bytesOfFileReceived = 0;
                        free(dataForAnalysis);
						gfc_cleanup(gfr);
                        return -1; 
					}
						
                
                    if (bytesRead > 10) {
                        char *statusText = strtok(NULL, " \r\n");
                        if (strcmp(statusText, "OK") != 0) {
                            if (strcmp(statusText, "FILE_NOT_FOUND") == 0) {
                                gfr->statusText = statusText;
                                gfr->status = gfc_get_status(gfr);
                                if (gfr->headerargument != NULL) {
                                    gfr->headerfunc("GETFILE FILE_NOT_FOUND", strlen("GETFILE FILE_NOT_FOUND"), gfr->headerargument);
                                }
								free(dataForAnalysis);
								gfc_cleanup(gfr);
                                return 0;
                            }
                            else if (strcmp(statusText, "ERROR") == 0) {
                                gfr->statusText = statusText;
                                gfr->status = gfc_get_status(gfr);
                                if (gfr->headerargument != NULL) {
                                    gfr->headerfunc("GETFILE ERROR", strlen("GETFILE ERROR"), gfr->headerargument);
                                }
                                free(dataForAnalysis);
								gfc_cleanup(gfr);
                                return 0;
                            }
                            else {
                                gfr->statusText = "INVALID";
                                gfr->status = gfc_get_status(gfr);
                                gfr->bytesOfFileReceived = 0;
                                free(dataForAnalysis);
								gfc_cleanup(gfr);
                                return -1; 
                            
                            }
                        }
                            
                        //receive OK Response
                        else {
                            
                            fprintf(stdout, "OK response received\n");
                            fflush(stdout);
                            gfr->statusText = statusText;
                            gfr->status = gfc_get_status(gfr);
                        
                            char *everythingAfterStatus = strtok(NULL, "");
                        
                            char *filelength = strtok(everythingAfterStatus, " \r\n");
                            char *filecontent = buffer + strlen(filelength) + 15;
                            
                            
                            

                        
                            if (filelength != NULL) {
 //                               fprintf(stdout,"filelength is:%s.\n", filelength);
                                gfr->filelengthstring = filelength;
                                gfr->filelength = atol(filelength);
                            
                                headerComplete = 1;
                            } else {
                                gfr->statusText = "INVALID";
                                gfr->status = gfc_get_status(gfr);
                                gfr->bytesOfFileReceived = 0;
                                free(dataForAnalysis);
								gfc_cleanup(gfr);
                                return -1;
                            }
                         
                          
                            if (filecontent != NULL) {                           
                                size_t filecontentlength = bytesRead - strlen(filelength) - 15;
                                gfr->writerfunc(filecontent, filecontentlength, gfr->writerargument); 
 //                               fprintf(stdout, "file content length is %d.\n", strlen(filecontent));
//                                fprintf(stdout, "file content length calculated is %d.\n", filecontentlength);
                                bytesTotalOfFile += filecontentlength;
                                gfr->bytesOfFileReceived = bytesTotalOfFile;
 //                               fprintf(stdout, "Bytes of File (actual): %zu. Bytes of File (theoretical): %zu.\n", bytesTotalOfFile, gfr->filelength);
 //                               fflush(stdout);
                                
                                
                            }
 
                            free(dataForAnalysis);
                            if (bytesTotalOfFile == gfr->filelength) {
                                fprintf(stdout, "Transfer succesfull and connection stopped. Bytes total: %zu.\n", bytesTotalOfFile);
                                close(clientSocket);
                                gfc_cleanup(gfr);
                                return 0;
                            }else {
                                continue;
                            }
                        }
                      } 
                    }
            
                }
            
            else {
//               fprintf(stdout, "File transfer not header message.\n");
//                fflush(stdout);
            
                bytesTotalOfFile = bytesTotalOfFile + bytesRead;
                gfr->writerfunc(buffer, bytesRead, gfr->writerargument);
                gfr->bytesOfFileReceived = bytesTotalOfFile;
//                fprintf(stdout, "Bytes of File (actual): %zu. Bytes of File (theoretical): %zu.\n", bytesTotalOfFile, gfr->filelength);
//                fflush(stdout);
                if (bytesTotalOfFile == gfr->filelength) {
                    close(clientSocket);
                    fprintf(stdout, "Transfer succesfull and connection stopped. Bytes total: %zu.\n", bytesTotalOfFile);
					gfc_cleanup(gfr);
                    return 0;
                }
            }
        }
        
    }
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr){
    gfstatus_t status;
    int result;
    
    if ((result = strcmp(gfr->statusText, "OK")) == 0) {
        status = GF_OK;
    }
    else if ((result = strcmp(gfr->statusText, "FILE_NOT_FOUND")) == 0) {
        status = GF_FILE_NOT_FOUND;
    }
    else if ((result = strcmp(gfr->statusText, "ERROR")) == 0) {
        status = GF_ERROR;
    }
    else {
        status = GF_INVALID;
    }
    
    return status;
}

char* gfc_strstatus(gfstatus_t status){
    char* strstatus;
    
    if (status == GF_OK) {
        strstatus = "OK";
    }
    else if (status == GF_FILE_NOT_FOUND) {
        strstatus = "FILE_NOT_FOUND";
    }
    else if (status == GF_ERROR) {
        strstatus = "ERROR";
    }
    else {
        strstatus = "INVALID";
    }
    
    return strstatus;
}

size_t gfc_get_filelen(gfcrequest_t *gfr){
    return gfr->filelength;
}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr){
    
    return gfr->bytesOfFileReceived;
}

void gfc_cleanup(gfcrequest_t *gfr){
	if (gfr != NULL)
		free(gfr);
}


void gfc_global_init(){
}

void gfc_global_cleanup(){
}