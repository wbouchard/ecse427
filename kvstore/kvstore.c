#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
    char *key;
    char *value;
} KvPair; 

/* Pod: has a key and stores several key-value pairs. */
typedef struct {
    int index;
    int num_of_kvpairs;
    KvPair *kvpair_m;
} Pod; 

/* SharedMem: is an array of Pods, each with several kv-pairs. */
typedef struct {
    char name[50];
    char addr[256];
    int size;
    int num_of_pods;
    Pod *pod_m;
} SharedMem;

int kv_store_create(char *name, SharedMem *shm);
int kv_store_write(char *key, char *value, SharedMem *shm);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);
int kv_delete_db(char *name);

main() {
    SharedMem shm;

    // allocate memory for shm elements
    shm.num_of_pods = MAX_POD_NUM;
    shm.pod_m = malloc(sizeof(Pod) * shm.num_of_pods);
    int i;
    for (i = 0; i < shm.num_of_pods; i++) {
        shm.pod_m [i].index = i;
        shm.pod_m [i].num_of_kvpairs = MAX_POD_STORE;
        shm.pod_m [i].kvpair_m 
            = malloc(sizeof(KvPair) * shm.pod_m [i].num_of_kvpairs); 
    }

    // shm_open needs a slash at the start of the name, 
    // and no other slashes in the name
    char *SHM_NAME = "/wb_shm";

    if (kv_store_create(SHM_NAME, &shm) == 0)
        kv_store_write("yohoho", "hello i am a write", &shm);
    kv_delete_db(SHM_NAME);

    // free memory used by shm elements
    for (i = 0; i < shm.num_of_pods; i++)
        free(shm.pod_m [i].kvpair_m);
    free(shm.pod_m);
}

/* kv_store_create: create a key-value store or open an existing one
 *                  return 0 on success, -1 if no memory available or
 *                  insufficient permissions */
int kv_store_create(char *storeName, SharedMem *shm) {
    strcpy(shm->name, storeName);
    shm->size = MAX_POD_NUM * MAX_POD_STORE * MAX_DATA_SIZE;

    int fd = shm_open(shm->name, O_CREAT|O_RDWR, S_IRWXU);
    printf("heres the create fd %d\n", fd);
    if (fd < 0) return -1;

    printf("size is %d\n", shm->size);
    char *addr = mmap(NULL, (size_t)shm->size, PROT_READ|PROT_WRITE, 
            MAP_SHARED, fd, 0);
    if (addr == (void *)-1) {
        printf("fuck mmap\n");
        printf("errno %s\n", strerror(errno));
        return -1;
    }

    strcpy(shm->addr, addr);
    printf("stored shm addr is %s\n", shm->addr);

    // truncate fd to be exactly as big as needed
    ftruncate(fd, shm->size);
    close(fd);

    return 0;
}

/* kv_store_write:  write a key-value pair to the store. */
int kv_store_write(char *key, char *value, SharedMem *shm) {
    if (strlen(key) > MAX_KEY_SIZE)
        key[MAX_KEY_SIZE] = '\0';
    if (strlen(value) > MAX_DATA_SIZE)
        value[MAX_DATA_SIZE] = '\0';

    int index = hash_func(key);
    int i;
    for (i = 0; i < shm->pod_m[index].num_of_kvpairs; i++) {
        if (shm->pod_m[index].kvpair_m[i].value == NULL) {
            printf("im writing\n");
            shm->pod_m[index].kvpair_m[i].key = key;
            shm->pod_m[index].kvpair_m[i].value = value;
            printf("key %s\n", shm->pod_m[index].kvpair_m[i].key);
            printf("val %s\n", shm->pod_m[index].kvpair_m[i].value);
            break;
        }
    }

    printf("addr for memcpy is %s\n", shm->addr);
    //memcpy(shm->addr, shm, shm->size);
    printf("did the memcpy\n");
    return 0;
}

/* kv_store_read:   takes a key and searches the store for the kv pair.
 *                  returns a pointer to the string */
char *kv_store_read(char *key) {
}

/* kv_store_read_all:   returns all values in the store that match the
 *                      given key */
char **kv_store_read_all(char *key) {
}

/* kv_delete_db:    delete the shared memory and all semaphores
 *                  return 0 on success, -1 on error */
int kv_delete_db(char *storeName) {
    return shm_unlink(storeName);
}

int hash_func(char *word){
    int hashAddress = 5381;
    int counter;
    for (counter = 0; word[counter]!='\0'; counter++) {
        hashAddress = ((hashAddress << 5) + hashAddress) + word[counter];
    }
    return hashAddress % MAX_POD_NUM < 0 ? -hashAddress % MAX_POD_NUM : hashAddress % MAX_POD_NUM;
}
