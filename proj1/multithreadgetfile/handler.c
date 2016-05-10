#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include "steque.h"

#include "gfserver.h"
#include "content.h"

#define BUFFER_SIZE 4096

struct task{
gfcontext_t * ctx;
char *path;
};


extern char *content;
steque_t workq;
pthread_mutex_t workqlock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t newrq_cond = PTHREAD_COND_INITIALIZER;

//make the following global in order to be cleaned up in the end
struct task *t;



ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg){
		
	fprintf(stdout, "got a request.\n");
	
	pthread_mutex_lock(&workqlock);
	
	t = malloc(sizeof(struct task));
	assert(t != NULL);
    t->ctx = ctx;
    t->path = path;
	// fprintf(stdout, "receive request from connection %d, request path:%s.\n", , p);    
     steque_enqueue(&workq, t);
	 pthread_mutex_unlock(&workqlock);
	 pthread_cond_broadcast(&newrq_cond);

    return 1;
}
	

ssize_t worker_main(void *arg){
	int id = *(int *) arg;
	int fildes;
	ssize_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[BUFFER_SIZE];
	struct task *job;
	gfcontext_t *ctx;
	char *path;
 //   content_init(content);	
	fprintf(stdout, "thread#%d is working .\n", id);
	
	while (1){
	 
	pthread_mutex_lock(&workqlock);
	while (steque_isempty(&workq)){
		pthread_cond_wait(&newrq_cond, &workqlock);
	}
	job = (struct task *)steque_pop(&workq);
	pthread_mutex_unlock(&workqlock);
	ctx = job -> ctx;
	path = job -> path;
	
	fprintf(stdout, "thread#%d is working on request path:%s.\n", id, path);
	
	if( 0 > (fildes = content_get(path)))
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);

	/* Calculating the file size */
	file_len = lseek(fildes, 0, SEEK_END);

	gfs_sendheader(ctx, GF_OK, file_len);

	/* Sending the file contents chunk by chunk. */
	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			gfs_sendheader(ctx, GF_ERROR, 0);
			gfs_abort(ctx);
			
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			gfs_sendheader(ctx, GF_ERROR, 0);
			gfs_abort(ctx);
	//		return -1;
		}
		bytes_transferred += write_len;
	}
	
	}
	return bytes_transferred;
//	content_destroy();
	
}	
	
	
