#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

typedef enum socket_type {
	LISTENER,
	PEER,
	UNBOUND
}socket_type;

//forward..

typedef struct socket_control_block SOCKET_CB;


typedef struct Listener_Socket{
  rlnode request_queue; //queue that has the requests for peer to peer connection

  CondVar listener_CV;


}lsocket;


typedef struct Peer_Socket {

  //pointer to the connected socket
  SOCKET_CB* socket;
  
  PIPE_CB* server; //sends data
  
  PIPE_CB* client;//receives data
} psocket;



typedef struct socket_control_block{
	int refcount;

	FCB* fcb;
	Fid_t fid;
	socket_type stype;
	port_t port;

	//peer or listener
	union {
    	psocket peer; 
    	lsocket listener;
  	};

	
}SOCKET_CB;


typedef struct request_connection{
  SOCKET_CB* requesting_socket; //socket that made the request...

  CondVar request_cv;
  
  int admited;
  
  rlnode node; // the node we add in the request_queue...
  
} socket_request;




SOCKET_CB* PORT_MAP[MAX_PORT+1]; //legal SOCKET_CB for listeners only!

int socket_read(void* read,char*buffer , unsigned int size);
int socket_write(void* write,const char*buffer , unsigned int size);
int socket_close(void* fid);

//SOCKET FILE OPS...

file_ops SOCKET_FOPS={
	.Open=NULL,
	.Read=socket_read,
	.Write=socket_write,
	.Close = socket_close
};


static inline void init_port_map(){

      for(int i=0; i<=MAX_PORT+1;i++){
      	PORT_MAP[i]=NULL;
    }
}


//just malloc and init the pipe_cb wuth spesific reader and writer dont call FCB_reserve
PIPE_CB* acquire_PIPE_CB(FCB** fcb,Fid_t* fid);

//decrease and free socket_cb....
void decrease_ref_count(SOCKET_CB* socket_cb);