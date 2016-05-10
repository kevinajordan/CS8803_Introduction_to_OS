/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */
 
struct minifyjpeg_input {
  opaque src<>;
};

struct minifyjpeg_output {
  opaque dst<>;
};

program MINIFYJPEG_PROG { 
  version MINIFYJPEG_VERS {
    minifyjpeg_output MINIFYJPEG_PROC(minifyjpeg_input) = 1; 
  } = 1; 
} = 0x31234567; 
