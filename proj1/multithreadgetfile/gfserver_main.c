#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "steque.h"
//#include "handler.c"

#include "gfserver.h"
#include "content.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  gfserver_main [options]\n"                                                      \
"options:\n"                                                                  \
"  -p [listen_port]    Listen port (Default: 8888)\n"                         \
"  -t [nthreads]       Number of threads (Default: 1)\n"                      \
"  -c [content_file]   Content file mapping keys to content files\n"          \
"  -h                  Show this help message.\n"                              

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"content",       required_argument,      NULL,           'c'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t worker_main(void *arg);
extern steque_t workq;

extern struct task *t;

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  unsigned short port = 8888;
  char *content = "content.txt";
  gfserver_t *gfs;
  int nthreads = 1;

  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:t:c:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
        break;
      case 'c': // file-path
        content = optarg;
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
  
  content_init(content);

	steque_init(&workq);

  /*Initializing server*/
  gfs = gfserver_create();

  /*Setting options*/
  gfserver_set_port(gfs, port);
  gfserver_set_maxpending(gfs, 100);
  gfserver_set_handler(gfs, handler_get);
  gfserver_set_handlerarg(gfs, NULL);
	
	
  

  //pthread_t worker[nthreads];	
	
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
	
	
	

	
	
  /*Loops forever*/
  gfserver_serve(gfs);
	
	if (t != NULL)
		free(t);
	
	
  // destroy content file descriptor 	
  content_destroy();
	
}