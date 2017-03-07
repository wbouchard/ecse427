#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

#define MAX_POD_NUM     256
#define MAX_POD_STORE   256
#define MAX_KEY_SIZE    32
#define MAX_DATA_SIZE   256

/* System calls
 *  shm_open
 *  mmap
 *  ftruncate
 *  memcpy
 *  fstat
 *  sem_open: sem_wait, sem_post
 */

/* Tips
 *  DO NOT USE MUTEX for this hw
 *  semaphores cannot be <0 
 *  semaphores not reentrant in C, can create locks
 *  compile with -pthreads flag 
 *  for unnamed semaphore, use sem_destroy
 *  for named semaphore, use sem_close and sem_unlink (try using sighandler) 
 *  make sure to munmap before unlinking shared memory w/ shm_unlink
 *  do not use pointers inside shared memory
 *  make a shared mem struct 
 *      inside create char[number of pod][max size of data]
 *      create an int[]     <-- what for? key storage? pod index storage?
 *  deal with collisions?
 *      if yes add annother char[][] array
 *  make sure pods are circular when searching through them
 *
 *  IMPLEMENT READ WRITE ON A SINGLE THREAD
 *  DEAL WITH SEMAPHORES AND RACE CONDITIONS AFTER
 */

/* Key value pair */
typedef struct KvPairs {
    int key;
    char value[MAX_DATA_SIZE];
} KvPair;

/* Pod: has a key and stores several key-value pairs. */
typedef struct Pods {
    int key;
    KvPair kvpairArray[MAX_POD_STORE];
} Pod;

/* SharedMem: is an array of Pods, each with several kv-pairs */
typedef struct SharedMems {
    char *name;
    Pod podArray[MAX_POD_NUM];
} SharedMem;

int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);

main() {
    // shm_open needs a slash at the start of the name, and no other
    // slashes in the name
    kv_store_create("/wb_mem");
}

/* kv_store_create: create a key-value store or open an existing one
 *                  return 0 on success, -1 if no memory available or
 *                  insufficient permissions */
int kv_store_create(char *storeName) {
    SharedMem sm;

    strcpy(sm.name, storeName);
    int fd = shm_open(sm.name, O_CREAT|O_EXCL|O_RDWR, S_IRWXU);
    if (fd < 0) return -1;

    int sm_size = MAX_POD_NUM * MAX_POD_STORE * MAX_DATA_SIZE;
    char *addr = mmap(NULL, sm_size, PROT_READ | PROT_WRITE, 
            MAP_SHARED, fd, 0);
    if (addr == (void *)-1) return -1;

    // set length of new shared memory object
    ftruncate(fd, sm_size);
    close(fd);

    // TODO: figure out how to give store location info to caller
    return 0;
}

/* kv_store_write:  write a key-value pair to the store. */
int kv_store_write(char *key, char *value) {
    // TODO: what should be done with evicted entries?
}
