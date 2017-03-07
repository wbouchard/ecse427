#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_POD_NUM     256
#define MAX_POD_STORE   256
#define MAX_KEY_SIZE    32
#define MAX_DATA_SIZE   256

/* Key value pair */
typedef struct {
    int key;
    char value[MAX_DATA_SIZE];
} KvPair; 

/* Pod: has a key and stores several key-value pairs. */
typedef struct {
    int key;
    int num_of_kvpairs;
    struct KvPair *indiv_kvpair;
} Pod; 

/* SharedMem: is an array of Pods, each with several kv-pairs */
typedef struct {
    char name[50];
    int num_of_pods;
    struct Pod *indiv_pod;
} SharedMem;

main() {
    SharedMem sm;
    sm.num_of_pods = MAX_POD_NUM;
    sm.indiv_pod = malloc(sizeof(Pod) * sm.num_of_pods);
    strcpy(sm.name, "aaaaaaaa");
    
    printf("%s\n", sm.name);
    free(sm.indiv_pod);
}
