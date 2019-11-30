
#include "tinyos.h"
#include "kernel_socket.h"

Fid_t sys_Socket(port_t port)
{	
	//illegal port...
	if (port < 0 || port > MAX_PORT) { 
		
		return NOFILE;
	}


	Fid_t fid = -1;
	FCB* fcb = NULL;
	
	if (FCB_reserve(1, &fid, &fcb)==0) // file ids of curproc are exhausted.
		return NOFILE;


	SOCKET_CB* socket_cb = (SOCKET_CB*)xmalloc(sizeof(SOCKET_CB)); //streamobj
	socket_cb -> fcb = fcb;
	socket_cb -> fid = fid;
	socket_cb -> stype = UNBOUND;
	socket_cb -> port = port;

	fcb -> streamobj = socket_cb;
	fcb -> streamfunc = &SOCKET_FOPS;

	return fid;
}

int sys_Listen(Fid_t sock)
{
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

int socket_read(void* read,char*buffer , unsigned int size){
	return -1;
}
int socket_write(void* write,const char*buffer , unsigned int size){
	return -1;
}
int socket_close(void* fid){
	return -1;
}
