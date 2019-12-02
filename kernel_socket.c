
#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_streams.h"

int socket_counter=0;

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


	if (socket_counter == 0)
	{
		init_port_map();
	}

	SOCKET_CB* socket_cb = (SOCKET_CB*)xmalloc(sizeof(SOCKET_CB)); //streamobj
	socket_cb -> fcb = fcb;
	socket_cb -> fid = fid;
	socket_cb -> stype = UNBOUND;
	socket_cb -> port = port;

	


	fcb -> streamobj = socket_cb;
	fcb -> streamfunc = &SOCKET_FOPS;

	socket_counter++;
	return fid;
}

int sys_Listen(Fid_t sock)
{
	FCB* fcb = get_fcb(sock);

 	if (fcb == NULL) //the file id is not legal
 		return -1;

 	SOCKET_CB* socket_cb = fcb -> streamobj;

 	if(socket_cb == NULL || socket_cb -> port <= 0 || socket_cb-> port >MAX_PORT+1)
 		return -1;

	if(PORT_MAP[socket_cb -> port] != NULL) // if this port has already a listener...
		return -1;

	if (socket_cb -> stype != UNBOUND)
		return -1;
	

	socket_cb -> stype = LISTENER;

	// initialize listener....
	
	socket_cb -> listener.listener_CV =COND_INIT;

	rlnode_init(& socket_cb->listener.request_queue, NULL);

	PORT_MAP[socket_cb -> port] = socket_cb;

	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	FCB* fcb = get_fcb(lsock);

	if (fcb == NULL) //the file id is not legal
 		return NOFILE;


 	SOCKET_CB* socket_cb = fcb -> streamobj;

 	if(socket_cb == NULL)
 		return NOFILE;

 	if(socket_cb -> stype != LISTENER)
 		return NOFILE;
 	//as long as socket_cb is a LISTENER dont need to check for valid port or if he is bounded to a PORT

 	while(rlist_len(&socket_cb->listener.request_queue)==0) // waiting for a request
		kernel_wait(&socket_cb -> listener.listener_CV,SCHED_PIPE);

	//get the request

	socket_request* request= rlist_pop_front(&socket_cb->listener.request_queue) -> socket_request ;

	//get the listenning socket
	SOCKET_CB* peer1_socket = request -> requesting_socket;

	if (peer1_socket == NULL) //listennig socket was closed!
		return NOFILE;

	//create a new socket in the same port
	Fid_t peer2_socket_fid = sys_Socket(peer1_socket->port); //already have the mutex

	FCB* peer2_socket_fcb = get_fcb(peer2_socket_fid);

	if (peer2_socket_fcb == NULL)
		return NOFILE;

	SOCKET_CB* peer2_socket = peer2_socket_fcb -> streamobj;

	peer1_socket -> stype = PEER;
	peer2_socket -> stype = PEER;


	//we have already Fid_t and FCB* dont want to reserve new ones=> dont call sys_Pipe()

	//one way comunication
	FCB* fcb_pipe1[2];
	fcb_pipe1[0] = peer1_socket->fcb;
	fcb_pipe1[1] = peer2_socket->fcb;

	Fid_t fid_pipe1[2];
	fid_pipe1[0] = peer1_socket -> fid;
	fid_pipe1[1] = peer2_socket -> fid;

	
	PIPE_CB* pipe1 = acquire_PIPE_CB(fcb_pipe1,fid_pipe1);

	//other way comunication

	FCB* fcb_pipe2[2];
	fcb_pipe2[0] = peer2_socket->fcb;
	fcb_pipe2[1] = peer1_socket->fcb;

	Fid_t fid_pipe2[2];
	fid_pipe2[0] = peer2_socket -> fid;
	fid_pipe2[1] = peer1_socket -> fid;

	
	PIPE_CB* pipe2 = acquire_PIPE_CB(fcb_pipe2,fid_pipe2);

	if(pipe1 == NULL || pipe2 == NULL)
		return NOFILE;

	//initialize peers sockets and connect them...

	//init first socket
	peer1_socket -> peer.socket = peer2_socket;
	peer1_socket -> peer.server = pipe1;
	peer1_socket -> peer.client = pipe2;

	//init second socket...
	peer2_socket -> peer.socket = peer1_socket;
	peer2_socket -> peer.server = pipe2;
	peer2_socket -> peer.client = pipe1;

	//...

	// request has been taken  care of!
	request -> admited = 1;

	kernel_signal(&request -> request_cv);



	return peer2_socket -> fid;
}
//just malloc and init the pipe_cb wuth spesific reader and writer dont call FCB_reserve
PIPE_CB* acquire_PIPE_CB(FCB** fcb,Fid_t* fid){

	PIPE_CB* pipe_cb =  (PIPE_CB*) xmalloc(sizeof(PIPE_CB));
	pipe_cb -> read_bytes=0;
	pipe_cb -> written_bytes=0;

	pipe_cb -> has_space= COND_INIT;
	pipe_cb -> has_data= COND_INIT;

	pipe_cb -> reader = fid[0];
	pipe_cb -> writer = fid[1];
	pipe_cb -> fcb_r = fcb[0];
	pipe_cb -> fcb_w = fcb[1];


	return pipe_cb;

}

int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{


	FCB* fcb = get_fcb(sock);

	if(fcb == NULL) //illegal Fid_t
		return -1;

	SOCKET_CB* socket_cb = fcb -> streamobj;

	if(socket_cb == NULL)
		return -1;

	if (port< 0 || port > MAX_PORT) // illegal port
		return -1;

	//only UNBOUND socket can connect
	//inorder to connect must be a listener to the input port
	if(socket_cb -> stype != UNBOUND || PORT_MAP[port] == NULL)
		return -1;

	SOCKET_CB* listener_sock = PORT_MAP[port];

	//create the request...
	socket_request* request = (socket_request*) xmalloc(sizeof(socket_request));

	//init the request
	request -> admited =0;
	request -> request_cv = COND_INIT;
	request -> requesting_socket = socket_cb;
	rlnode_init(&request -> node,request); 

	// add the new node into the request_queue of listener
	rlist_push_back(&listener_sock-> listener.request_queue, &request -> node);

	kernel_signal(&listener_sock->listener.listener_CV); //wake up the listener because the request is ready!

	//wait for a spesific time...
	int success = kernel_timedwait(&request->request_cv, SCHED_USER, timeout);

	rlist_remove(&request -> node);

	if(!success) //time out!
		return -1;

	if(!request -> admited) //request wasnt accepted
		return -1;

	return 0;
		
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

int socket_read(void* read,char*buffer , unsigned int size){
	SOCKET_CB* socket_cb = (SOCKET_CB*) read;

	if(socket_cb -> stype != PEER)
		return -1;

	int bytes = pipe_read(socket_cb -> peer.client,buffer,size);
	return bytes;
}
int socket_write(void* write,const char*buffer , unsigned int size){
	SOCKET_CB* socket_cb = (SOCKET_CB*) write;

	if(socket_cb -> stype != PEER)
		return -1;
	int bytes = pipe_write(socket_cb -> peer.server,buffer ,size);
	return bytes;


}
int socket_close(void* fid){
	SOCKET_CB* socket_cb = (SOCKET_CB*) fid;
	if(socket_cb == NULL)
		return -1;
	return -1;
}
