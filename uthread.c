#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "user.h"
#include "proc.h"
#include "uthread.h"
static int n_elements;
int
uthread_init()
{
    // init process threads table
    int i, j;
    for (i = 0; i < MAX_THREAD; i++) {
        ttable.threads[i] = (struct uthread *) malloc(sizeof(struct uthread));
        if(ttable.threads[i] < 0) {
            return -1;  // malloc has failed
        }
        ttable.threads[i]->tid = i;
        ttable.threads[i]->state = T_TERMINATED;
        for (j = 0; j < MAX_THREAD; j++) {
            ttable.threads[i]->waiting_threads[j] = 0;  // no one is waiting for thread i to terminate yet
        }
    }
    // create the main thread
    // ttable.threads[0]->stack = (void *) malloc(STACK_SIZE); // allocate memory for the main thread stack
    // if(ttable.threads[0]->stack < 0) {
    //     return -1;  // malloc has failed
    // }
    // PLACE_ESP_IN(ttable.threads[0]->esp);
    // PLACE_EBP_IN(ttable.threads[0]->ebp);
    curr_t = ttable.threads[0]; // main thread is the current running thread
    curr_t->state = T_RUNNING;  // main thread is running
    curr_t->sleep_ticks = 0;    // no need to sleep yet
    // register SIGALRM to uthread_schedule function
    signal(SIGALRM, &uthread_schedule);
    // execute alarm
    alarm(THREAD_QUANTA);
    return 0;
}

int
uthread_create(void (*start_func)(void *), void* arg)
{
  int i;
  for (i = 0; i < MAX_THREAD; i++) {
    if(ttable.threads[i]->state == T_TERMINATED) {  // uninitialized thread found
        ttable.threads[i]->stack = (void *) malloc(STACK_SIZE);   // allocate memory for the thread stack
        if(ttable.threads[i]->stack < 0) {
            return -1;  // malloc has failed
        }
        // ttable.threads[i]->esp = (uint) ttable.threads[i]->stack;
        // ttable.threads[i]->esp += STACK_SIZE; // the thread esp now points to the bottom of the allocated stack

        ttable.threads[i]->tf = (struct trapframe*) malloc(sizeof(struct trapframe));   // allocate memory for trapframe
        if(ttable.threads[i]->tf < 0) {
            return -1;  // malloc has failed
        }
        // thread esp = thread ebp = bottom of stack
        ttable.threads[i]->tf->esp = (uint) ttable.threads[i]->stack + STACK_SIZE;
        ttable.threads[i]->tf->ebp = ttable.threads[i]->tf->esp;

        uint curr_esp; // temp variable to save the current esp in
        PLACE_ESP_IN(curr_esp);
        UPDATE_ESP_WITH(ttable.threads[i]->tf->esp);    // esp now equals the thread stack esp

        PUSH(arg);    // push the start_func argument onto the thread stack
        PUSH(&uthread_exit);  // push the exit function as the return address
        // PUSH(start_func); //push the start_func to be executed first
        ttable.threads[i]->tf->eip = (uint) start_func;

        // PLACE_ESP_IN(ttable.threads[i]->esp); //update esp in threads[i]
        // PUSH(ttable.threads[i]->esp); //push esp
        // PLACE_ESP_IN(ttable.threads[i]->esp); //update esp in threads[i]
        // ttable.threads[i]->ebp = ttable.threads[i]->esp; //update ebp to esp
        UPDATE_ESP_WITH(curr_esp); //restore the current esp
        ttable.threads[i]->state = T_READY;
        return ttable.threads[i]->tid;
    }
  }
  return -1;
}

void
uthread_schedule()
{
    // back up trapframe
    int i;
    uint curr_esp, shift_tf;
    PLACE_ESP_IN(curr_esp);
    shift_tf = 4 + 4 + 7 + sizeof(struct trapframe);  // how much to shift esp un til saved trapframe from 1.4
    curr_esp += shift_tf;
    *curr_t->tf = *((struct trapframe*) curr_esp);   // copy the current process trapframe to the current thread trapframe
    // select thread to run - round robin policy
    for(i = curr_t->tid + 1; i < MAX_THREAD; i++) {
        if(ttable.threads[i]->state == T_READY && (uptime() - ttable.threads[i]->start_of_sleep) > ttable.threads[i]->sleep_ticks) {   // found a thread ready for running
            switch_threads(ttable.threads[i], curr_esp);
            return;
        }
    }
    for(i = 0; i <= curr_t->tid; i++) {
        if(ttable.threads[i]->state == T_READY) {   // found a thread ready for running
            switch_threads(ttable.threads[i], curr_esp);
            return;
        }
    }
    alarm(THREAD_QUANTA);   // keep changing threads every THREAD_QUANTA
}

void
uthread_exit()
{
    int i;
     // wake up all the threads that are waiting for this thread to terminate
    for(i = 0; i < MAX_THREAD; i++) {
        if(curr_t->waiting_threads[i] != 0) {
            ttable.threads[i]->state = T_READY;
            curr_t->waiting_threads[i] = 0;
        }
    }
    // free thread resources
    free(curr_t->tf);
    free(curr_t->stack);
    // determine if a ready thread exists
    for(i = 0; i < MAX_THREAD; i++) {
        if(ttable.threads[i]->state == T_READY) {   // found a thread ready for running
            alarm(0);   // context switch
            return;
        }
    }
    // no ready threads remain
    for(i = 0; i < MAX_THREAD; i++) {
        free(ttable.threads[i]);    // free threads table
    }
    exit();
}

int
uthread_self()
{
  return curr_t->tid;
}

int
uthread_join(int tid)
{
    if (tid < 0 || tid >= MAX_THREAD) { // invalid tid
        return -1;
    }
    int i;
    struct uthread *t = 0;
    // get the desired thread
    for (i = 0; i < MAX_THREAD; i++) {
        if(ttable.threads[i]->tid == tid) {
            t = ttable.threads[i];
            break;
        }
    }
    if(t == 0) {
        return -1;  // no such thread
    }
    // block until the desired thread is terminated, or return immediately if is already terminated
    if (t->state != T_TERMINATED) {
        t->waiting_threads[curr_t->tid] = 1;    // mark the current thread as waiting for the desired thread to terminate
        curr_t->state = T_BLOCKED;  // the current thread is now blocked
        alarm(0);   // context switch
    }
    return 0;
}

int
uthred_sleep(int ticks)
{
    curr_t->sleep_ticks = ticks;
    curr_t->start_of_sleep = uptime();
    curr_t->state = T_BLOCKED;
    alarm(0);   // context switch
    return 0;
}

void
switch_threads(struct uthread *next_thread, uint curr_esp)
{
    curr_t->state = T_READY;    // current thread is now ready
    next_thread->state = T_RUNNING; // the next thread is now running
    curr_t = next_thread;   // make the next thread current
    *((struct trapframe*) curr_esp) = *curr_t->tf;   // copy the trapframe from the next thread to the process stack
}

void
update_thread_sleep_ticks()
{
    int i;
    for (i = 0; i < MAX_THREAD; i++) {
        if(ttable.threads[i]->sleep_ticks > 0) {
            ttable.threads[i]->sleep_ticks -= 1;
        }
    }
}


int bsem_alloc(){
    BINSEM *bin_sem;
    bin_sem =(BINSEM *) malloc (sizeof (BINSEM));
    bin_sem->threadsQueue=0;
    bin_sem->value=0;
    bin_sem->locked=0;
    bin_sem_arr[n_elements] == bin_sem;
    return &bin_sem;
}
void 
bsem_free(int descriptor)
{
    /*
    int i= 0;
    for(; i<n_elements; i++)
    if(descriptor == bin_sem_arr[n_elements])
        free(curr_sem->threadsQueue);
        free(curr_sem);
    */
    free ((BINSEM )descriptor); // not sure about that
}
void  bsem_down
(int )
{
    while(xchg(&semaphore->locked, 1) != 0); //wait for the lock to be yours
    if (semaphore->value == 1) {
    semaphore->value--;
    xchg(&semaphore->locked, 0); //release the lock
  } else if (semaphore->value == 0) 
  {
    enqueueToSem(&semaphore->threadsQueue, currThread);
    currThread->state = T_SLEEPING; //change this thread state to sleeping
    alarm(0); //set off the alarm for this thread as we are handling it
    xchg(&semaphore->locked, 0); //release the lock
    uthread_yield(); //yield this thread
  } else 
  {
    xchg(&semaphore->locked, 0);
    printf(2, "semaphore down!\n");
    exit();
  }   
}
void bsem_up
(int)
{
    while(xchg(&semaphore->locked, 1) != 0); //wait for the lock to be yours
    if (semaphore->value == 0) {
    struct uthread* waiting = dequeueToSem(&semaphore->threadsQueue);
    if (waiting == 0) { //no one is waiting
      semaphore->value++;
    } else { //somebody is waiting
      waiting->state = T_RUNNABLE;
      enqueue(&threadsQueue, waiting);
    }
    xchg(&semaphore->locked, 0); //release the lock
  } else {
    xchg(&semaphore->locked, 0); //release the lock
  }
}
struct uthread* enqueueToSem(struct uthread **head, struct uthread* t) {
  if (*head != 0) {
    struct uthread* current = *head;
    while (current->nextWaiting != 0)
      current = current->nextWaiting;
    current->nextWaiting = t;
  } else {
    *head = t;
  }
  t->nextWaiting = 0;
  return *head;
}
struct uthread* dequeueToSem(struct uthread **head) {
  struct uthread* result = 0;
  if (*head != 0) {
    result = *head;
    *head = (*head)->nextWaiting;
    result->nextWaiting = 0;
  }
  return result;
}







int csem_alloc(int Sem_num)
{
  COUNT_SEMAPHORE* count_sem;
  count_sem = (struct semaphore*)malloc(sizeof(struct semaphore));
  count_sem->s1 = bsem_alloc();
  count_sem->s2 = bsem_alloc();
  bsem_up(s2);
  count_sem->value = Sem_num;
  return &count_sem;
}
void csem_free(){
}    
   
void 
down
(struct counting_semaphore *sem)
{
  bsem_down(sem->s2);
  bsem_down(sem->s1);
  sem->value--;
  if(sem->value > 0)
    bsem_up(sem->s2);
  bsem_up(sem->s1);
}
void 
up
(struct  counting_semaphore  *sem)
{
  bsem_down(sem->s1);
  sem->value++;
  if(sem->value == 1)
    bsem_up(sem->s2);
  bsem_up(sem->s1);
}
