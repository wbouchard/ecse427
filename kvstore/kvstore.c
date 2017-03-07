#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
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
 *  deal with collisions?
 *      if yes add annother char[][] array
 *  make sure pods are circular when searching through them
 *
 *  IMPLEMENT READ WRITE ON A SINGLE THREAD
 *  DEAL WITH SEMAPHORES AND RACE CONDITIONS AFTER
 */

/* Key value pair */
typedef struct {
    int key;
    char value[MAX_DATA_SIZE];
} KvPair; 

/* Pod: has a key and stores several key-value pairs. */
typedef struct {
    int key;
    int num_of_kvpairs;
    KvPair *indiv_kvpair;
} Pod; 

/* SharedMem: is an array of Pods, each with several kv-pairs. */
typedef struct {
    char name[50];
    int num_of_pods;
    Pod *indiv_pod;
} SharedMem;

int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);

main() {
    // shm_open needs a slash at the start of the name, 
    // and no other slashes in the name
    kv_store_create("/wb_mem");
}

/* kv_store_create: create a key-value store or open an existing one
 *                  return 0 on success, -1 if no memory available or
 *                  insufficient permissions */
int kv_store_create(char *storeName) {
    SharedMem shm;

    // allocate memory for shm elements
    shm.num_of_pods = MAX_POD_NUM;
    shm.indiv_pod = malloc(sizeof(Pod) * shm.num_of_pods);
    int i;
    for (i = 0; i < shm.num_of_pods; i++) {
        shm.indiv_pod [i].num_of_kvpairs = MAX_POD_STORE;
        shm.indiv_pod [i].indiv_kvpair 
            = malloc(sizeof(KvPair) * shm.indiv_pod [i].num_of_kvpairs); 
    }
    strcpy(shm.name, storeName);

    int shm_size = MAX_POD_NUM * MAX_POD_STORE * MAX_DATA_SIZE;

    int fd = shm_open(shm.name, O_CREAT|O_EXCL|O_RDWR, S_IRWXU);
    if (fd < 0) return -1;

    char *addr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, 
            MAP_SHARED, fd, 0);
    if (addr == (void *)-1) return -1;

    // truncate fd to be exactly as big as needed
    ftruncate(fd, shm_size);
    close(fd);

    // free memorey for shm elements
    free(shm.indiv_pod);
    for (i = 0; i < shm.num_of_pods; i++)
        free(shm.indiv_pod [i].indiv_kvpair);
    return 0;
}

/* kv_store_write:  write a key-value pair to the store. */
int kv_store_write(char *key, char *value) {
    // TODO: what should be done with evicted entries?
}
