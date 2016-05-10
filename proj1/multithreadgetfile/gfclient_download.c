#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include "steque.h"

#include "workload.h"
#include "gfclient.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -p [server_port]    Server port (Default: 8888)\n"                         \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \
"  -t [nthreads]       Number of threads (Default 1)\n"                       \
"  -n [num_requests]   Requests download per thread (Default: 1)\n"           \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"server",        required_argument,      NULL,           's'},
  {"port",          required_argument,      NULL,           'p'},
  {"workload-path", required_argument,      NULL,           'w'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"nrequests",     required_argument,      NULL,           'n'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

static void Usage() {
	fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while(NULL != (cur = strchr(prev+1, '/'))){
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)){
      if (errno != EEXIST){
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if( NULL == (ans = fopen(&path[0], "w"))){
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
  FILE *file = (FILE*) arg;

  fwrite(data, 1, data_len, file);
}


// variables for multithread

steque_t workq;
pthread_mutex_t workqlock = PTHREAD_MUTEX_INITIALIZER;

int nrequests = 1;
char *server = "localhost";
unsigned short port = 8888;
char *workload_path = "workload.txt";
int i;
int option_char = 0; 
int nthreads = 1;
char *req_path;


  
  







/* worker main program */
void * worker_main(void *arg){
	int id = *(int *) arg;
	char * treq_path;
	FILE *file;
	char local_path[512];
	gfcrequest_t *gfr;
    
	int returncode = 0;
	
//	gfr = gfc_create();
 //   gfc_set_server(gfr, server);
	
	
	for (int i = 0; i < nrequests; i++){
		fprintf(stdout, "-------------\n");
		fflush(stdout);
        
		
		pthread_mutex_lock(&workqlock);
		if (steque_isempty(&workq)) {
            pthread_mutex_unlock(&workqlock);
            break;
        }
		treq_path = (char *)steque_pop(&workq);
		
		
		
		if(treq_path == NULL){
			break;
		}
	    fprintf(stdout, "thread #%i working on request #%i with file path:%s.\n", id, i, treq_path);
		fflush(stdout);
		
		

    	localPath(treq_path, local_path);
		file = openFile(local_path);
		gfr = gfc_create();
    	gfc_set_server(gfr, server);
    	gfc_set_path(gfr, treq_path);
    	gfc_set_port(gfr, port);
    	gfc_set_writefunc(gfr, writecb);
    	gfc_set_writearg(gfr, file);

    	fprintf(stdout, "thread #%i requesting %s%s\n", id, server, treq_path);
		fflush(stdout);
		
        pthread_mutex_unlock(&workqlock);
    	if ( 0 > (returncode = gfc_perform(gfr))){
      	fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
		
			
     	 fclose(file);
      	if ( 0 > unlink(local_path))
        fprintf(stderr, "unlink failed on %s\n", local_path);
   		 }
    	else {
        	fclose(file);
    	}
		

    	if ( gfc_get_status(gfr) != GF_OK){
      		if ( 0 > unlink(local_path))
        	fprintf(stderr, "unlink failed on %s\n", local_path);
    	}
		

    	fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
    	fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));
		// clean up resource		
		gfc_cleanup(gfr);
		if (gfr != NULL)
			free(gfr);
	}
}



/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */

  
	
	
   	
	
	
	

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "s:p:w:n:t:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 's': // server
        server = optarg;
        break;
      case 'p': // port
        port = atoi(optarg);
        break;
      case 'w': // workload-path
        workload_path = optarg;
        break;
      case 'n': // nrequests
        nrequests = atoi(optarg);
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
  //      if(nthreads != 1){
   //       fprintf(stderr, "Multiple threads not yet supported.\n");
   //       exit(0);
//        }
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

  if( EXIT_SUCCESS != workload_init(workload_path)){
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }
	
	
	

  gfc_global_init();
	
	
	
  // make request and enqueue	
	
	steque_init(&workq);

  /*Making the requests...*/
  for(i = 0; i < nrequests * nthreads; i++){
    req_path = workload_get_path();
	

    if(strlen(req_path) > 256){
      fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
      exit(EXIT_FAILURE);
    }
	steque_enqueue(&workq, req_path);
  }
	
	int size = steque_size(&workq);
	fprintf(stdout, "work queue size is :%d.\n", size);
	  

	  
	// creat worker threads
	pthread_t worker[nthreads];
	int no_threads = 0;
	int workerID[nthreads];
	for (int i = 0; i < nthreads; i++){
		workerID[i] = i;
		if (0 != pthread_create(&worker[i], NULL, worker_main, &workerID[i])) {
            fprintf(stderr, "could not create worker thread #%i\n",
                    workerID[i]);
            break;
        }
        no_threads++;
    }
	
	
	
	 // join all the worker threads
    for (int i = 0; i < no_threads; i++) {
        if (0 != pthread_join(worker[i], NULL)) {
            fprintf(stderr, "could not join worker thread #%i\n", workerID[i]);
        }
    }

 
  

  gfc_global_cleanup();

  return 0;
}  
