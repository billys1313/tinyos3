
#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

//writer file ops struct
file_ops pipe_writer_stream_func ={
	.Open  = pipe_invalid_open,
	.Read  = pipe_invalid_read,
	.Write = pipe_write,
	.Close = close_pipe_writer
};

//reader file ops struct
file_ops pipe_reader_stream_func ={
	.Open  = pipe_invalid_open,
	.Read  = pipe_read,
	.Write = pipe_invalid_write,
	.Close = close_pipe_reader
};

/*
Returns by reference pipe_t
pipe-> read will be the Fid_t (file file descriptor) to the reader FCB
Reader FCB will have stream_func-> pipe_reader_stream_func

pipe-> write will be the Fid_t (file file descriptor) to the writer FCB
Writer FCB will have stream_func-> pipe_writer_stream_func

Both FCB's will have streamobj->PIPE_CB
*/

int sys_Pipe(pipe_t* pipe) 
{	
	Fid_t fid[2];
	FCB* fcb[2];
		

	if (FCB_reserve(2, fid, fcb)==0)
		return -1;

	PIPE_CB* pipe_cb = (PIPE_CB*)xmalloc(sizeof(PIPE_CB)); //streamobj


	//init pipe_cb
	pipe_cb -> reader= fid[0];
	pipe_cb -> writer= fid[1];
	pipe_cb -> fcb_w= fcb[0];
	pipe_cb -> fcb_r= fcb[1];


	pipe_cb -> read_bytes=0;
	pipe_cb -> written_bytes=0;

	pipe_cb -> has_space= COND_INIT;
	pipe_cb -> has_data= COND_INIT;

	//attach pipe_cb to reader fcb,writer fcb
	fcb[0]->streamobj=pipe_cb;
	fcb[1]->streamobj=pipe_cb;

	//attach to reader fcb reader file_ops
	fcb[0]->streamfunc=&pipe_reader_stream_func;

	//attach to writer fcb writer file_ops
	fcb[1]->streamfunc=&pipe_writer_stream_func;

	
	
	pipe->read=fid[0];
	pipe->write=fid[1];
	return 0;
}

//pipe_read will read from pipe_cb->buffer into input buffer
//on success returns the number of read bytes

int pipe_read(void* reader,char* buffer,unsigned int size){
	PIPE_CB* pipe_cb = (PIPE_CB*) reader;

	int available_bytes= pipe_cb->written_bytes - pipe_cb->read_bytes;	// available bytes to read

	


	int until;		//how many bytes can we read

	if(pipe_cb->fcb_r==NULL )  //cant read without reader!
		return -1;

	while (available_bytes==0) {

		if(pipe_cb -> fcb_w == NULL) // EOF no writer and no bytes
			return 0;
		
		kernel_broadcast(&pipe_cb -> has_space);				//wake up the writers!
		kernel_wait(&pipe_cb -> has_data,SCHED_PIPE);			//wait for bytes
		available_bytes = pipe_cb->written_bytes - pipe_cb->read_bytes;
	}

	//start reading...
	
	until = (size <= available_bytes) ? size : available_bytes;

	for (int i = 0; i < until; ++i){

		buffer[i] = pipe_cb -> buffer [pipe_cb -> read_bytes % PIPE_BUFFER_SIZE]; //cycle
		pipe_cb -> read_bytes ++;

		
	}
	kernel_broadcast(&pipe_cb -> has_space);
	//available_bytes -= until;
	

	return until;
}
//pipe_read will write input buffer to pipe_cb->buffer
//on success returns the number of bytes copied 

int pipe_write(void* writer,const char* buffer,unsigned int size){
	PIPE_CB* pipe_cb = (PIPE_CB*) writer;

	int free_bytes= PIPE_BUFFER_SIZE -(pipe_cb->written_bytes - pipe_cb->read_bytes);	// free space


	int until;		//how many bytes cam we read

	if(pipe_cb->fcb_r==NULL || pipe_cb->fcb_w==NULL)  
		return -1;

	while(free_bytes == 0){

		kernel_broadcast(&pipe_cb -> has_data);				//wake up readers
		kernel_wait(&pipe_cb -> has_space, SCHED_PIPE);
		free_bytes= PIPE_BUFFER_SIZE -(pipe_cb->written_bytes - pipe_cb->read_bytes);

	}

	until=(size <= free_bytes )? size : free_bytes;

	for (int i = 0; i < until; ++i){


		pipe_cb -> buffer[pipe_cb -> written_bytes % PIPE_BUFFER_SIZE] = buffer[i];
		pipe_cb -> written_bytes++;
		
	}
	kernel_broadcast(&pipe_cb -> has_data);
	
	
	return until;

}

int close_pipe_reader(void* fid){
	PIPE_CB* pipe_cb = (PIPE_CB*) fid;

	pipe_cb -> fcb_r = NULL;
	pipe_cb  -> reader =-1;

	if(pipe_cb -> fcb_w == NULL){
		free(pipe_cb);
		return 0;
	}
	else{
		kernel_broadcast(&pipe_cb -> has_space);
		return 0;
	}
	

}
int close_pipe_writer(void* fid){
	PIPE_CB* pipe_cb = (PIPE_CB*) fid;

	pipe_cb -> fcb_w = NULL;
	pipe_cb  ->  writer =-1;

	if(pipe_cb -> fcb_r == NULL){
		free(pipe_cb);
		return 0;
	}
	else{
		kernel_broadcast(&pipe_cb -> has_data);
		return 0;
	}
	

	
}

//Invalid operations!!
int pipe_invalid_read(void* reader,char* buffer,unsigned int size){	return -1;	}

int pipe_invalid_write(void* writer,const char* buffer,unsigned int size){	return -1;	}

void* pipe_invalid_open(unsigned int minor){	return (void*)-1;	}