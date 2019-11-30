#include "tinyos.h"
#include "kernel_streams.h"

typedef enum socket_type {
	LISTENER,
	PEER,
	UNBOUND
}socket_type;


typedef struct socket_control_block{

	FCB* fcb;
	Fid_t fid;
	socket_type stype;
	port_t port;
	
}SOCKET_CB;




SOCKET_CB* PORT_MAP[MAX_PORT+1]; //legal SOCKET_CB

int socket_read(void* read,char*buffer , unsigned int size);
int socket_write(void* write,const char*buffer , unsigned int size);
int socket_close(void* fid);


//SOCKET FILE OPS...

file_ops UNBOUND_FOPS={
	.Open=NULL,
	.Read=NULL,
	.Write=NULL,
	.Close = socket_close
};