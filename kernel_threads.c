
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"
#include "assert.h"
#include "kernel_streams.h"

//consider Tid_t as pointer to ptcb




PTCB* search_ptcb(Tid_t tid){

  PTCB* ptcb = (PTCB*)tid;


  if(rlist_find(&CURPROC -> thread_list, ptcb, NULL))
    return ptcb;
  else
    return NULL;

}
//returns the current PTCB by searching the threads of CURPROC

PTCB* getCurrentPTCB(){ return search_ptcb( (Tid_t)CURTHREAD->ptcb_tcb) ; }

//same with start_main_thread in kernel_proc.c

void start_thread()
{
  int exitval;
 
  PTCB* curr_ptcb = CURTHREAD -> ptcb_tcb;

  Task call =  curr_ptcb -> main_task;
  int argl = curr_ptcb -> argl;
  void* args = curr_ptcb -> args;

  exitval = call(argl,args);
  ThreadExit(exitval);
}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  if (task != NULL)
  {
      assert(CURPROC -> thread_count>=1) ;// CURPROC has already the main thread!
      PTCB* ptcb = spawn_ptcb(CURPROC,task,argl,args);


      TCB* thread_tcb = spawn_thread(CURPROC, start_thread);

      /* more initialiazes */
      
      ptcb -> tcb = thread_tcb;      // and initialize the ptcb's tcb;
      thread_tcb -> ptcb_tcb = ptcb;
      

      //spawn_thread create a thread with in INIT state ,so we call wakeup to chnage it to READY!
      wakeup(ptcb->tcb); 
      
      return (Tid_t) ptcb;

    

  }
  
  return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
  return (Tid_t) CURTHREAD-> ptcb_tcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
  

  PTCB* ptcb= search_ptcb(tid);

  //if ptcb == null then PTCB doesn't exists! or it is in another process.
  if (ptcb == NULL){
    return -1;
  }     
  //can't join
  if(ptcb -> is_exited == EXITED_STATE || ptcb -> is_detached == DETACH || (Tid_t)CURTHREAD == tid  ){
    return -1 ;
  }

  ptcb->refcount++; // increase the joining ptcbs

  // until ptcb is exitd or detached waiting
  while(ptcb->is_exited != EXITED_STATE && ptcb->is_detached != DETACH ){
    kernel_wait(&ptcb->cond_var,SCHED_USER);
  }

  ptcb->refcount--;// decrease the joining ptcbs

  if(ptcb->is_detached==DETACH){
    return -1 ;
  }
  

  if(exitval!=NULL)
    *exitval=ptcb->exit_val;

  //thread is exited here!
  if (ptcb -> refcount == 0 ){ 
    rlist_remove(&ptcb->thread_list_node);
    free(ptcb);

  }

  


  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{

   //search PTCB with the given tid
   PTCB* ptcb = search_ptcb( tid );

   //PTCB doesn't exists! or it is in another process.
   if(ptcb == NULL){ 

      return -1; 
   }


   if (ptcb -> is_exited == EXITED_STATE)
   {
      return -1;
   }

   ptcb -> is_detached = DETACH;
   kernel_broadcast(&ptcb -> cond_var); // let all the threads know that the thread with tid is DETACHED
   return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  PTCB* ptcb = CURTHREAD -> ptcb_tcb;
  // curthread always exists! just to be sure!
  if(ptcb == NULL){ 

    return ; 
  }
  
   
  
  CURPROC -> thread_count--;          // decrease the threads of curproc

  ptcb -> main_task = NULL;
  ptcb -> exit_val = exitval;
  ptcb -> is_exited = EXITED_STATE;
 
  kernel_broadcast(&ptcb -> cond_var); // let all the threads know that the CURTHREAD is EXITED

  

  if (  CURPROC -> thread_count == 0){  // clean PCB if there aren't any other threads! Like sys_Exit
    
    cleanup_pcb();  //fuction in kernel_proc.c
    
  }

  // there aren't any others joinning threads and the thread is exited free ptcb
  if (ptcb -> refcount == 0 ){ 
    rlist_remove(&ptcb->thread_list_node);
    free(ptcb);

  }

  /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER); // gain will free TCB
  

}


