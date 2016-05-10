#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"

void* minify_via_rpc(CLIENT* cl, void* src_val, size_t src_len, size_t *dst_len) {

	minifyjpeg_output *result;
    minifyjpeg_input input;
	enum clnt_stat retval;

	input.src.src_len = src_len;
	input.src.src_val = src_val;
	
	result = (minifyjpeg_output *)malloc(sizeof(minifyjpeg_output));
	result->dst.dst_val = (char*)calloc(src_len, sizeof(char));
//	&(result->dst.dst_len) = malloc(sizeof(src_len));
	
//	bzero(result, sizeof(minifyjpeg_output));
//	bzero(result->dst.dst_val, sizeof(char) * src_len);
//	bzero(&(result->dst.dst_len), sizeof(src_len));
	
	
	result->dst.dst_len = 0;
	
	retval = minifyjpeg_proc_1(input, result, cl);

	

    if (retval != RPC_SUCCESS) {
      clnt_perror(cl, "Error");
 //     clnt_destroy(cl);
	//  free(result->dst.dst_val);
	//  free(result);
	  
      exit(1);

    } else {
      *dst_len = result->dst.dst_len;
	  return result->dst.dst_val;
    }


}


CLIENT* get_minify_client(char *server) {
     CLIENT *cl;
    /* Your code here */
	cl = clnt_create(server, MINIFYJPEG_PROG, MINIFYJPEG_VERS, "tcp");
	if (cl == (CLIENT *)NULL) {
      clnt_pcreateerror(server);
      return NULL;
     }
    return cl;
}
