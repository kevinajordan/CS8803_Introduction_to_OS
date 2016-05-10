#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <mqueue.h>
#include <sys/mman.h>


#include "gfserver.h"
#include "shm_channel.h"
#include "steque.h"

#define SHAREDMPATH "/sharedmpath"
#define MSGQID "/my_msgq"


typedef struct msgq_data_t {
        int segment_size;
		char path[256];
        char fldes[256];		
} msgq_data;

typedef struct shm_data_t {
        int bytes_written;
        int file_length;
        pthread_mutex_t data_mutex;
        pthread_mutex_t file_len_mutex;
        pthread_cond_t cache_cond;
        pthread_cond_t proxy_cond;
} shm_data;

char* segment_id;
extern int segments;
extern int segment_size;

int fldes_queue_size;
int max_fldes_queue_size;

steque_t fldes_queue;

pthread_mutex_t fldes_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t msgq_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t write_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t read_cond = PTHREAD_COND_INITIALIZER;

/*
Create and initialize the shared memory segment based on the file description string from the message queue.

 */

shm_data* create_shm_channel(char* fldes) {
		shm_unlink(fldes);
//		fprintf(stdout, "fldes is: %s.\n", fldes);
        int shm_fldes = shm_open(fldes, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (shm_fldes < 0) {
                fprintf(stderr, "Error %d (%s) on server proxy shm_open.\n", errno, strerror(errno));
				fflush(stdout);
 //               exit(1);
        }

        ftruncate(shm_fldes, segment_size);
        shm_data* mem = mmap(NULL, segment_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fldes, 0);
        close(shm_fldes);

        if(mem->file_length != 0 || mem->bytes_written != 0) {
          fprintf(stderr, "shared memory segment should have been set zero.\n");
		  fflush(stdout);
   //       exit(1);
        }


        mem->file_length = 0;
        mem->bytes_written = 0;

// initialize shared memory mutex and condition variables		
		
        pthread_mutexattr_t memory_attr;
		pthread_mutexattr_init(&memory_attr);
        pthread_mutexattr_setpshared(&memory_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mem->data_mutex, &memory_attr);
		
		pthread_mutexattr_t filelen_attr;
		pthread_mutexattr_init(&filelen_attr);
        pthread_mutexattr_setpshared(&filelen_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mem->file_len_mutex, &filelen_attr);
		
        pthread_condattr_t cache_attr;
		pthread_condattr_init(&cache_attr);
        pthread_condattr_setpshared(&cache_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&mem->cache_cond, &cache_attr);
		
		pthread_condattr_t proxy_attr;
		pthread_condattr_init(&proxy_attr);
        pthread_condattr_setpshared(&proxy_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&mem->proxy_cond, &proxy_attr);
        
        return mem;
}


//clean up shared memory segment by resetting the memory content and enqueue it to the file description queue

void destroy_shm_seg(char* fldes, shm_data* mem) {
        //release everything in our shm object
        pthread_mutex_destroy(&mem->data_mutex);
        pthread_cond_destroy(&mem->proxy_cond);
        pthread_cond_destroy(&mem->cache_cond);
        mem->file_length = 0;
        mem->bytes_written = 0;

        if (munmap(mem, segment_size) < 0) {
                fprintf(stdout, "Error %d (%s) on proxy munmap.\n", errno, strerror(errno));
				fflush(stdout);
        }

        if (shm_unlink(fldes) < 0) {
                fprintf(stdout, "Error %d (%s) on proxy shm_unlink.\n", errno, strerror(errno));
				fflush(stdout);
        }

        pthread_mutex_lock(&fldes_mutex);
        while (fldes_queue_size == max_fldes_queue_size) {
                pthread_cond_wait(&write_cond, &fldes_mutex);
        }
		steque_enqueue(&fldes_queue, fldes);
        fldes_queue_size++;

        pthread_mutex_unlock(&fldes_mutex);
        pthread_cond_broadcast(&read_cond);
		//pthread_cond_signal(&read_cond);
}

void init_proxy() {
		
		 steque_init(&fldes_queue);
		
		// create segment

		
		for (int i = 0; i < segments; i++) {
			
			char str_id[16] = "";

			sprintf(str_id, "%d", i);
			segment_id = (char*) malloc((strlen(str_id) + strlen(SHAREDMPATH) + 1) * sizeof(char));
			bzero(segment_id, (strlen(str_id) + strlen(SHAREDMPATH) + 1) * sizeof(char));
			strcpy(segment_id, SHAREDMPATH);
			strcat(segment_id, str_id);

			shm_unlink(segment_id);
			fprintf(stdout, "Created Segment ID: %s\n", segment_id);
			fflush(stdout);
			steque_enqueue(&fldes_queue, segment_id);
		}
	
		fldes_queue_size = steque_size(&fldes_queue);
        max_fldes_queue_size = steque_size(&fldes_queue);
//		fprintf(stdout, "init queue done.\n");

}

ssize_t handle_with_cache(gfcontext_t* ctx, char *path, void* arg) {
        char* fldes;

        pthread_mutex_lock(&fldes_mutex);
        while (fldes_queue_size == 0) {
                pthread_cond_wait(&read_cond, &fldes_mutex);
        }
        fldes = (char*)steque_pop(&fldes_queue);
        fldes_queue_size--;
//		fprintf(stdout, "fldes is:%s.", fldes);
        pthread_mutex_unlock(&fldes_mutex);
        pthread_cond_broadcast(&write_cond);
		//pthread_cond_signal(&write_cond);



        shm_data* mem = create_shm_channel(fldes);

        mqd_t mq;

        do {
                mq = mq_open (MSGQID, O_RDWR);
        } while(mq == (mqd_t) -1 || mq == 0);

        msgq_data data;
        strcpy(data.path, path);
        strcpy(data.fldes, fldes);
        data.segment_size = segment_size;

 //       pthread_mutex_lock(&msgq_mutex);

        int msg_rsp = mq_send(mq, (const char *) &data, 1024, 1);
        if (msg_rsp < 0) {
                fprintf(stderr, "Error %d (%s) on server proxy mq_send.\n", errno, strerror(errno));
				fflush(stdout);
  //              exit(1);
        }
        mq_close(mq);
 //       pthread_mutex_unlock(&msgq_mutex);

        pthread_mutex_lock(&mem->file_len_mutex);
        while(mem->file_length == 0) {
			pthread_cond_wait(&mem->proxy_cond, &mem->file_len_mutex);
        }

        int file_len = mem->file_length;

        pthread_mutex_unlock(&mem->file_len_mutex);
        pthread_cond_broadcast(&mem->cache_cond);

 //       fprintf(stdout, "Received file length is: %d\n", file_len);
//        fflush(stdout);
        
        if(file_len < 0) {
 //               fprintf(stdout, "File not found in cache\n");
//				fflush(stdout);
                destroy_shm_seg(fldes, mem);
                return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        }else {

                gfs_sendheader(ctx, GF_OK, file_len);

             
                int bytes_transferred = 0;
                int write_len = 0;
                char *data_start = (void *)(mem + 1);
                while (bytes_transferred < file_len) {

                        pthread_mutex_lock(&mem->data_mutex);
                        while(mem->bytes_written == 0) {
                                pthread_cond_wait(&mem->proxy_cond, &mem->data_mutex);
                        }
                        int read_len = mem->bytes_written;
                        write_len = gfs_send(ctx, data_start, read_len);
                        if (write_len != read_len) {
                                fprintf(stderr, "handle_with_cache write error");
                                return EXIT_FAILURE;
                        }
                        mem->bytes_written = 0;

                        pthread_mutex_unlock(&mem->data_mutex);
                        pthread_cond_broadcast(&mem->cache_cond);
                        bytes_transferred += write_len;
                }


                destroy_shm_seg(fldes, mem);
                fflush(stdout);
                return bytes_transferred;
        }
}
