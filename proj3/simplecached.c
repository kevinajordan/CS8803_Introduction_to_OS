#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <errno.h>
#include <semaphore.h>

#include "shm_channel.h"
#include "simplecache.h"
#include "steque.h"

#define MAX_CACHE_REQUEST_LEN 256
#define MSGQID "/my_msgq"



typedef struct msgq_data_t {
        int segment_size;
        char path[256];
        char fldes[256]; 
} msgq_data;

typedef struct shm_data_t {
        int bytes_written;
        int file_length;
        pthread_mutex_t file_len_mutex;
        pthread_mutex_t data_mutex;
        pthread_cond_t cache_cond;
        pthread_cond_t proxy_cond;
} shm_data; 

steque_t mq_data_queue;
int steq_size = 0;

pthread_mutex_t msgq_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t write_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t read_cond = PTHREAD_COND_INITIALIZER;

//extern pthread_mutex_t msgq_mutex;

static void _sig_handler(int signo){
        if (signo == SIGINT || signo == SIGTERM) {
                mq_unlink(MSGQID);
                simplecache_destroy();
                exit(signo);
        }
}

#define USAGE                                                                 \
        "usage:\n"                                                                    \
        "  simplecached [options]\n"                                                  \
        "options:\n"                                                                  \
        "  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
        "  -c [cachedir]       Path to static files (Default: ./)\n"                  \
        "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"nthreads",           required_argument,      NULL,           't'},
        {"cachedir",           required_argument,      NULL,           'c'},
        {"help",               no_argument,            NULL,           'h'},
        {NULL,                 0,                      NULL,             0}
};

void Usage() {
        fprintf(stdout, "%s", USAGE);
}



void* cache_worker(void* threadArgument) {
        while (1) {
                int file_len = 0;
                int cache_fldes = 0;

                pthread_mutex_lock(&msgq_mutex);
                while (steq_size == 0) {
                  pthread_cond_wait(&write_cond, &msgq_mutex);
                }
                msgq_data* msg = (msgq_data*) steque_pop(&mq_data_queue);
                steq_size--;

                pthread_mutex_unlock(&msgq_mutex);
                pthread_cond_signal(&read_cond);
//				printf("Msg Path: %s\n", msg->path);
                cache_fldes = simplecache_get(msg->path);

                /* Calculating the file size */
                if (cache_fldes > -1) {
                        file_len = lseek(cache_fldes, 0, SEEK_END);
                        lseek(cache_fldes, 0, SEEK_SET);
						//fprintf(stdout, "Found requested file in the cache directory, file length: %d\n", file_len);
						fflush(stdout);
                } else {
					//	printf("Requested file is not in the cache directory.\n");
                        file_len = -1;
                }
                

				int shm_fldes = 0;
					do{
					shm_fldes = shm_open(msg->fldes, O_RDWR, 0);
					if (shm_fldes < 0) {
						printf("Error %d (%s) unable to open shared memory.\n", errno, strerror(errno));
						sleep(1);
					}

				}while(shm_fldes < 0);
				
                ftruncate(shm_fldes, msg->segment_size);
				
                //setup the shared memory segment 
				shm_data * mem;
                mem = mmap(NULL, msg->segment_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fldes, 0);
                if(mem == (void*) -1) {
                        fprintf(stderr, "Error %d (%s) on server cache mmap.\n", errno, strerror(errno));
						fflush(stdout);
                        exit(1);
                }
                close(shm_fldes);




                mem->file_length = 0;
                if(mem->file_length != 0) {
                        fprintf(stdout, "Memory segment should have been set zero.\n");
						fflush(stdout);
                        exit(1);
                }

                pthread_mutex_lock(&mem->file_len_mutex);
                while(mem->file_length != 0) {
						pthread_cond_wait(&mem->cache_cond, &mem->file_len_mutex);
                }

                mem->file_length = file_len;
                mem->bytes_written = 0;

                pthread_mutex_unlock(&mem->file_len_mutex);
                pthread_cond_broadcast(&mem->proxy_cond);


                if(cache_fldes < 0) {
 //                       fprintf(stdout, "Requested file is not in cache directory.\n\n");
//						fflush(stdout);
                        if ( munmap(mem, msg->segment_size)!= 0) {
								fprintf(stdout, "Error %d (%s) on munmap.\n", errno, strerror(errno));
								fflush(stdout);
								exit(1);
						}
                } else {
                        int bytes_written =0;
                        int read_len = 0;
                        char *data_start = (void *)(mem + 1);

                        while (bytes_written < file_len) {
                                pthread_mutex_lock(&mem->data_mutex);
                                while(mem->bytes_written != 0) {
                                        pthread_cond_wait(&mem->cache_cond, &mem->data_mutex);
                                }
                                read_len = pread(cache_fldes, data_start, (msg->segment_size - sizeof(shm_data)) - 1, bytes_written);
                                mem->bytes_written = read_len;
                                pthread_cond_signal(&mem->proxy_cond);
                                pthread_mutex_unlock(&mem->data_mutex);


                                bytes_written += read_len;
                        }
                        if ( munmap(mem, msg->segment_size)!= 0) {
								fprintf(stdout, "Error %d (%s) on munmap.\n", errno, strerror(errno));
								fflush(stdout);
								exit(1);
						}
//                      fprintf(stdout, "Finish sending cache file to proxy in shared memory. Total bytes sent: %d\n\n", bytes_written);
//						fflush(stdout);
                }
        }

}




int main(int argc, char **argv) {
        int nthreads = 1;
        char *cachedir = "locals.txt";
        char option_char;
        int bytes_received = 0;
		
		mqd_t m_queue;
		struct mq_attr msgq_attr;

        while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
                switch (option_char) {
                case 't': // thread-count
                        nthreads = atoi(optarg);
                        break;
                case 'c': //cache directory
                        cachedir = optarg;
                        break;
                case 'h': // help
                        Usage();
                        exit(0);
                        break;
                default:
                        Usage();
                        exit(1);
                }
        }

		
		
//close the message queue before daemon starts
        int rc = mq_unlink(MSGQID); 
        if (rc == 0) {
                fprintf (stdout, "Previous message queue is closed.\n");
				fflush(stdout);
        }
		
		msgq_attr.mq_flags = 0;
		msgq_attr.mq_maxmsg = 10;
		msgq_attr.mq_msgsize = 1024;
		msgq_attr.mq_curmsgs = 0;
		
        m_queue = mq_open(MSGQID, O_RDWR|O_CREAT|O_EXCL, 0666, &msgq_attr);

        if (m_queue == -1) {
                fprintf(stderr, "Error %d (%s) on server cache mq_open.\n", errno, strerror(errno));
				fflush(stdout);
                exit(1);
        }
		
		
        if (signal(SIGINT, _sig_handler) == SIG_ERR) {
                fprintf(stderr,"Can't catch SIGINT...exiting.\n");
				fflush(stdout);
                exit(EXIT_FAILURE);
        }

        if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
                fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
				fflush(stdout);
                exit(EXIT_FAILURE);
        }

        /* Initializing the cache */
        simplecache_init(cachedir);

        /* Initializing the message steque */
        steque_init(&mq_data_queue);

        //Create the worker threads
        pthread_t workerThreadIDs[nthreads];

        for(int i = 0; i < nthreads; i++) {
                fprintf(stdout, "Created Cache Thread: %d\n", i);
				fflush(stdout);
                if (pthread_create(&workerThreadIDs[i], NULL, cache_worker, NULL) != 0) {
                        fprintf(stderr, "Failed to create simplecached thread: %d\n", i);
						fflush(stdout);
                }
        }
		

        while (1) {
                msgq_data data;
                bytes_received = mq_receive(m_queue, (char *) &data, 1024, NULL);
                if (bytes_received < 0) {
                        fprintf(stderr, "Error %d (%s) on mq_receive.\n", errno, strerror(errno));
						fflush(stdout);
                        exit(1);
                }

                pthread_mutex_lock(&msgq_mutex);
                while(steq_size == MAX_CACHE_REQUEST_LEN) { 
                        pthread_cond_wait(&read_cond, &msgq_mutex);
                }
                steque_enqueue(&mq_data_queue, &data);
                steq_size++;
                pthread_mutex_unlock(&msgq_mutex);
                pthread_cond_broadcast(&write_cond);

        }

}
