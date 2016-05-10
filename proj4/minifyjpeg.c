#include <stdio.h>
#include <rpc/rpc.h>        /* always needed */
#include "minifyjpeg.h"
#include "magickminify.h"


/* Implement the needed server-side functions here */

bool_t minifyjpeg_proc_1_svc(minifyjpeg_input input, minifyjpeg_output * result, struct svc_req * reqest){
	ssize_t dst_len = 0;
	bool_t retval = 1;
	magickminify_init();
 	result->dst.dst_val = magickminify(input.src.src_val, input.src.src_len,&(result->dst.dst_len));
//	result->dst.dst_len = dst_len;
	 if (result->dst.dst_val == (void*)NULL) {
        printf("magickminify function returns NULL");
        exit(1);
    }
	magickminify_cleanup();
	return retval;
}

int minifyjpeg_prog_1_freeresult (SVCXPRT * transp, xdrproc_t xdr_minifyjpeg_output, caddr_t result){
//	 magickminify_cleanup();
 //   (void) xdr_free(xdr_minifyjpeg_output, result);
    return 1;
}
