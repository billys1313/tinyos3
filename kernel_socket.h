#include "tinyos.h"
#include "kernel_streams.h"

typedef enum socket_type {
	LISTENER,
	PEER,
	UNBOUND
}socket_type;

typedef struct Listener{
  rlnode request_queue; //queue that has the requests for peer to peer connection

  CondVar listener_CV;
}LISTENER;



typedef struct socket_control_block{

	FCB* fcb;
	Fid_t fid;
	socket_type stype;
	port_t port;

	//peer or listener
	union {
    	PEER peer; 
    	LISTENER listener;
  };
	
}SOCKET_CB;


typedef struct Peer {

  SOCKET_CB* socket;
  
  PIPE_CB* server; //sends data
  
  PIPE_CB* client;//receives data
} PEER;


typedef struct request_connection{
  SOCKET_CB* socket; //socket that made the request...

  
  int admited;
  
  
} socket_request;



SOCKET_CB* PORT_MAP[MAX_PORT+1]; //legal SOCKET_CB for listeners only!

int socket_read(void* read,char*buffer , unsigned int size);
int socket_write(void* write,const char*buffer , unsigned int size);
int peer_close(void* fid);

//SOCKET FILE OPS...

file_ops SOCKET_FOPS={
	.Open=NULL,
	.Read=socket_read,
	.Write=socket_write,
	.Close = socket_close
};

