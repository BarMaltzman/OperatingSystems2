#define MAX_THREAD  64
#define THREAD_QUANTA 5
#define STACK_SIZE  4096
#define MAX_BSEM    128
#define PLACE_ESP_IN(var) __asm__("movl %%esp, %0;" : "=r" (var))  // place the currrent esp in var
#define PLACE_EBP_IN(var) __asm__("movl %%ebp, %0;" : "=r" (var))  // place the currrent ebp in var
#define UPDATE_ESP_WITH(var) __asm__("movl %0, %%esp;" : : "r" (var)) // update esp with the contents of var
#define UPDATE_EBP_WITH(var) __asm__("movl %0, %%ebp;" : : "r" (var)) // update ebp with the contents of var
#define PUSH(var) __asm__("movl %0, %%edi; push %%edi;" : : "r" (var))  // push the content of var to the stack

typedef enum  {T_RUNNING, T_READY, T_BLOCKED, T_TERMINATED} uthread_state;

struct uthread {
	int            		tid;	// thread id
	void           		*stack;	// thread stack
	uthread_state  		state; 	// thread state
    struct trapframe 	*tf;  	// the context of the thread
	uint 				sleep_ticks;	// min number of ticks for the thread to sleep
	uint 				start_of_sleep;	// the tick when the thread started it's sleep
    char  		   		waiting_threads[MAX_THREAD];	// the tids of the threads that are waiting for this thread to terminate
};

struct ttable {
  struct uthread *threads[MAX_THREAD];  // all the threads in a single process
};

static struct uthread *curr_t;  // the current thread
static struct ttable ttable;	// threads table

int uthread_init();
int uthread_create(void (*start_func)(void *), void *arg);
void uthread_schedule();
void uthread_exit();
int uthred_self();
int uthred_join(int tid);
int uthred_sleep(int ticks);
void switch_threads(struct uthread *next_thread, uint curr_esp);
void update_thread_sleep_ticks();

/* ===================================================== *
 * ========== Binary Semaphores - ass2 task 3 part1 ==== *
 * ===================================================== */

typedef struct binary_semaphore {
  uint locked;       		/* Is the lock held? */
  int value; 	     		/*only 0 or 1 */
  struct uthread *threadsQueue; /*the threads which are waiting for this semaphore */
  int dscrptr;
}BINSEM;
BINSEM * bin_sem_arr[MAX_BSEM];
void bsem_down(int);
void bsem_up(int);
void binary_semaphore_up(struct binary_semaphore* semaphore);
int bsem_alloc();
void bsem_free(int descriptor);

/* ===================================================== *
 * ========== Counting Semaphores - ass2 task 3 part2=== *
 * ===================================================== */
typedef struct counting_semaphore{
      int s1; //binary_semaphore_ID
  int s2; //binary_semaphore_ID
  int value;
}COUNT_SEMAPHORE;
void csem_alloc(int Sem_num);
void down(struct counting_semaphore *sem);
void up(struct  counting_semaphore  *sem);
