#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

#define BUFFER_SIZE	4096

//Replace with an implementation of handle_with_curl and any other
//functions you may need.

//https://curl.haxx.se/libcurl/c/getinmemory.html

typedef struct memory_t{
  char *memory;
  size_t size;
}memory_t, * memory_ptr;

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
  size_t realsize = size * nmemb;
  memory_ptr mem = (memory_ptr)userdata;
 
  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), ptr, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;



}

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
	int offset;
	size_t bytes_transferred;
	double file_len;
	ssize_t write_len;
	char buffer[BUFFER_SIZE];
	char *data_dir = arg;

	strcpy(buffer,data_dir);
	strcat(buffer,path);
	
	CURL *curl_handle;
	CURLcode res, getinfo;
	
	
	memory_t chunk;
	
	chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk.size = 0;    /* no data at this point */
	
	// set global variables
	curl_global_init(CURL_GLOBAL_ALL);
	
	/* init the curl session */ 
	curl_handle = curl_easy_init();
 
	/* specify URL to get */ 
	curl_easy_setopt(curl_handle, CURLOPT_URL, buffer);
	
	/* send all data to this function  */ 
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
 
	/* we pass our 'chunk' struct to the callback function */ 
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
	
	/* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	
    /* request failure on HTTP response>=400*/
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
 
	/* get it! */ 
	res = curl_easy_perform(curl_handle);
    
	/* check for errors */ 
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);		
	}
	else {
        file_len = 0;
        getinfo = curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD, &file_len);
             
        if (getinfo != CURLE_OK) {
            fprintf(stderr, "curl_easy_getinfo() failed: %s\n", curl_easy_strerror(getinfo));         
            return EXIT_FAILURE;
        } else {           
            gfs_sendheader(ctx, GF_OK, file_len);
            
            /* Sending the file contents chunk by chunk. */
            bytes_transferred = 0;
            offset = 0;
            while(bytes_transferred < file_len){
                if (file_len - bytes_transferred >= BUFFER_SIZE) {
		            write_len = gfs_send(ctx, chunk.memory + offset, BUFFER_SIZE);
		        } else {
		            write_len = gfs_send(ctx, chunk.memory + offset, file_len - bytes_transferred);
		        }
		        if (write_len <= 0){
			        fprintf(stderr, "handle_with_curl write error");
			        return EXIT_FAILURE;
		        }
		        offset += write_len;
		        bytes_transferred += write_len;
	        }
        }
	}
	

	
	/* cleanup curl stuff */ 
	curl_easy_cleanup(curl_handle);
 
	free(chunk.memory);
 
	/* we're done with libcurl, so clean it up */ 
	curl_global_cleanup();
	
	return bytes_transferred;
}

