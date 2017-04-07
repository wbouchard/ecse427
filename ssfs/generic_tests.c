/* 
 * Generic tests: test parts of the full sfs_api.c file 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE	1024
#define NUM_BLOCKS	1024
#define MAX_INODES	72 
#define MAX_OPEN_FILES	20

typedef struct _inode_t {
	// ints are 32 bits long
	int size;
	int direct[14];
	int indirect;
} inode_t;

typedef struct _superblock_t {
	unsigned char magic[4]; // unsigned char is 8 bits = 1 byte
	int block_size;
	int file_system_size;
	int no_of_inodes;
	inode_t root; // j-node
	inode_t shadow[4];
	int last_shadow;
} superblock_t;

typedef struct _block_t {
	unsigned char bytes[BLOCK_SIZE];
} block_t;

typedef struct _bit_array_t {
	// 1024 bit vector is the same as 32 ints * 32 bits 
	int four_bytes[32];
} bit_array_t;

typedef struct _dir_entry_t {
	char filename[10];
	int inode_no;	// use int for easier inode # tracking; 4 bytes
	char filler[2]; // needed to reach 16 bytes
} dir_entry_t;

typedef struct _directory_t {
	int full;
	int req_blocks;
	inode_t files[MAX_INODES];
	dir_entry_t entries[MAX_INODES];
} directory_t;

typedef struct _fd_entry_t {
	int inode_no;
	inode_t inode;
	int read_ptr;
	int write_ptr;
} fd_entry_t;

typedef struct _open_fd_table_t {
	int full;
	fd_entry_t entries[MAX_OPEN_FILES];
} open_fd_table_t;

open_fd_table_t *ofdt;
directory_t *dir;
bit_array_t *FBM, *WM;

int get_next_free_block(int *bit_array);
int get_next_free_fd();
int get_next_free_dir();
void write_dir_to_disk();
void write_fbm_to_disk();
void write_wm_to_disk();
int bytes_to_blocks_rnd_up(int bytes);
int bytes_to_blocks_rnd_down(int bytes);
void write();

int main() {
	// bit_array_t *FBM, *WM;
	// directory_t *dir;
	// superblock_t *superblock;
	// inode_t *j_node;
	// inode_t *dir_node;

	// // allocate space for all blocks
	// ssfs = (ssfs_t*)calloc(1, NUM_BLOCKS*BLOCK_SIZE);

	// // initialize FBM and WM
	// FBM = (bit_array_t*)calloc(1, BLOCK_SIZE);
	// WM  = (bit_array_t*)calloc(1, BLOCK_SIZE);

	// // initialize FBM, WM to 1's (all empty, all writable)
	// for (int i = 0; i < NUM_BLOCKS; i++) {
	// 	setBit(FBM->four_bytes, i);
	// 	setBit(WM->four_bytes , i);
	// }

	// // initialize superblock and reserve the first block for it
	// superblock = (superblock_t*)calloc(1, BLOCK_SIZE);
	// superblock->block_size = BLOCK_SIZE;
	// superblock->file_system_size = NUM_BLOCKS;
	// superblock->no_of_inodes = 1;
	// int sb_index = get_next_free_block(FBM->four_bytes);
	// clrBit(FBM->four_bytes, sb_index);

	// // root node, points to all blocks containing i-nodes
	// j_node = (inode_t*)calloc(1, sizeof(inode_t));
	// j_node->size = sizeof(inode_t);

	// // the first i-node contains the directory
	// dir_node = (inode_t*)calloc(1, sizeof(inode_t));
	// dir_node->size = sizeof(inode_t);
	// dir = (directory_t*)calloc(1, sizeof(directory_t));
	// dir->full_entries = 0;

	// // test: add an entry to dir
	// dir_entry_t new_entry;
	// strcpy(new_entry.filename, "testo");
	// strcpy(new_entry.inode_no, "2");
	// dir->entries[dir->full_entries] = new_entry;

	// // find free blocks to copy dir to, and reserve them
	// double needed_blocks =  (double)sizeof(directory_t) / (double)(BLOCK_SIZE) ;
	// int nfb = 0;
	// for (int i = 0; i < needed_blocks; i++) {
	// 	nfb = get_next_free_block(FBM->four_bytes);
	// 	dir_node->direct[i] = nfb;
	// 	clrBit(FBM->four_bytes, nfb);
	// }

	// // copy dir_node to a block, and store the block's index in j_node
	// nfb = get_next_free_block(FBM->four_bytes);
	// memcpy(ssfs->blocks[nfb].bytes, dir_node, dir_node->size);
	// j_node->direct[0] = nfb;
	// getBit(FBM->four_bytes, nfb);

	// // add j-node to super block and copy super to 1st block
	// superblock->root = *j_node;
	// memcpy(ssfs->blocks[sb_index].bytes, superblock, BLOCK_SIZE);

	// // copy FBM to second to last block, and WM to last block
	// clrBit(FBM->four_bytes, NUM_BLOCKS-2);
	// clrBit(FBM->four_bytes, NUM_BLOCKS-1);
	// memcpy(ssfs->blocks[NUM_BLOCKS-2].bytes, FBM, BLOCK_SIZE);
	// memcpy(ssfs->blocks[NUM_BLOCKS-1].bytes, WM , BLOCK_SIZE);

	// // printf("FBM's bit 0 	= %d\n", getBit(FBM->four_bytes, 0));
	// // printf("FBM's bit 1 	= %d\n", getBit(FBM->four_bytes, 1));
	// // printf("FBM's bit 222 	= %d\n", getBit(FBM->four_bytes, 222));
	// // printf("FBM's bit 999 	= %d\n", getBit(FBM->four_bytes, 999));
	// // printf("FBM's bit 1000 	= %d\n", getBit(FBM->four_bytes, 1000));
	// // printf("FBM's bit 1022 	= %d\n", getBit(FBM->four_bytes, 1022));
	// // printf("FBM's bit 1023 	= %d\n", getBit(FBM->four_bytes, 1023));

	// printf("test print super: %d\n", superblock->block_size);
	// printf("test print block 0: %s\n", ssfs->blocks[0].bytes);
	// print_ssfs(ssfs, FBM->four_bytes);

	// double testd = 5.69;
	// int testi = (int)testd;
	// printf("test i = %d\n", testi);

	//write();

	int test = 1;

	for (int i = 2; i < 2 + test; i++) {
		printf("uh oh\n");
	}

	return 0;
}

void write() {
	// if (ofdt->full == 0) {
	// 	fprintf(stderr, "Error: No open file descriptors\n");
	// 	return -1;
	// }

	// if (fileID < 0 || fileID >= MAX_OPEN_FILES) {
	// 	fprintf(stderr, "Error: File descriptor %d out of bounds [0, %d]\n", fileID, MAX_OPEN_FILES);
	// 	return -1;
	// }
	// if (ofdt->entries[fileID].inode_no == -1) {
	// 	fprintf(stderr, "Error: No open file associated with file descriptor %d\n", fileID);
	// 	return -1;
	// }
	// if (length < 0) {
	// 	fprintf(stderr, "Error: Cannot write %d bytes from current write pointer location\n", length);
	// 	return -1;
	// }

	// int ino = ofdt->entries[fileID].inode_no;

	// // find the block in which we need to write
	// int write_block_loc = bytes_to_blocks_rnd_up(ofdt->entries[fileID].write_ptr) - 1; // round down (write inside a full block)
	
	// // find how many blocks the write operation requires
	// int req_blocks = bytes_to_blocks_rnd_up(length); // round up (need n free blocks)

	// // TODO: use indirect inode pointer to solve this
	// if (write_block_loc + req_blocks > 14) {
	// 	fprintf(stderr, "Error: File too big; cannot fit in free blocks\n");
	// 	return -1;
	// }

	// int free_blocks_in_mem = 0;
	// for (int i = 0; i < NUM_BLOCKS; i++)
	// 	if (getBit(FBM->four_bytes, i) == 1)
	// 		free_blocks_in_mem++;

	// if (write_block_loc + req_blocks > free_blocks_in_mem) {
	// 	fprintf(stderr, "Error: Filesystem too full to write file\n");
	// 	return -1;
	// }

	// if the write_ptr points to a full block, need to make a block from old+new data
	// that block is already existing and full in dir and FBM
	int write_ptr = 0;
	int length = 2048;
	char buf[2048];
	buf[0] = 'a';
	buf[2047] = '\0';
	block_t blocks_test[14];
	int write_block_loc = bytes_to_blocks_rnd_down(write_ptr);
	int req_blocks = bytes_to_blocks_rnd_up(length);
	int buf_ptr = 0;

	// int buf_ptr = 0;
	// if ((write_ptr % BLOCK_SIZE) != 0) {
	// 	block_t *partial_block = (block_t*)calloc(1, BLOCK_SIZE);
	// 	//read_blocks(dir->files[ino].direct[write_block_loc], 1, partial_block);


	// 	int wpos = ofdt->entries[fileID].write_ptr;
	// 	buf_ptr = BLOCK_SIZE - wpos;

	// 	memcpy(partial_block + wpos, buf + buf_ptr, buf_ptr);
		
	// 	//write_blocks(write_block_loc, 1, partial_block);
	// 	free(partial_block);
	// 	write_block_loc++;
	// 	req_blocks--;
	// 	dir->files[ino].size -= buf_ptr; // temp adjustment to reflect real size change later
	// }

	// write data in new blocks
	// int it = 0;
	// for (int i = write_block_loc; i < write_block_loc + req_blocks; i++) {
	// 	void *tmp = (void*)calloc(1, BLOCK_SIZE);

	// 	memcpy(tmp, buf + (buf_ptr + BLOCK_SIZE*it++), BLOCK_SIZE); // copy one block of buf at a time
		
	// 	tmp->bytes[1023] = '\0';

	// 	printf("block contents at i=%d: %s\n", i, tmp->bytes);

	// 	//write_blocks(nfb, 1, tmp);
	// 	free(tmp);
	// }

    printf("length = %d\n", length);

}

// void print_ssfs(ssfs_t *ssfs, int *bit_array) {
// 	for (int i = 0; i < NUM_BLOCKS; i++) {
// 		if (getBit(bit_array, i) == 0) {
// 			printf("at index i=%d, bit=%d\n", i, getBit(bit_array, i));
// 			ssfs->blocks[i].bytes[BLOCK_SIZE-1] = '\0';
// 			printf("contents are %s\n", ssfs->blocks[i].bytes);
// 		}
// 	}
// }

/* 
 * Functions that interact with the FBM and WM, which are bit arrays 
 * that use an array of ints for representation.
 * 	- setBit: set bit at bit index i to 1
 * 	- clrBit: set bit at bit index i to 0
 * 	- getBit: get bit at bit index i
 * adapted from http://www.mathcs.emory.edu/~cheung/Courses/255/Syllabus/1-C-intro/bit-array.html
 */
void setBit(int *bit_array, int i) {
	int index = i / sizeof(int);
	int bit_pos = i % sizeof(int);
	int set = 1;		  // 00000000 00000000 00000000 00000001
	set = set << bit_pos; // bitwise shift left by x positions

	//printf("setting int at %d to %d\n", index, array[index] | set);
	bit_array[index] = bit_array[index] | set; // binary or
}

void clrBit(int *bit_array, int i) {
	int index = i / sizeof(int);
	int bit_pos = i % sizeof(int);
	int set = 1;		  		// 00000000 00000000 00000000 00000001
	set = ~(set << bit_pos); 	// bitwise shift left by x positions, and invert
	bit_array[index] = bit_array[index] & set; // binary and
}

int getBit(int *bit_array, int i) {
	int index = i / sizeof(int);
	int bit_pos = i % sizeof(int);
	int set = 1;
	set = set << bit_pos;

    return ((bit_array[index] & set) != 0) ;
}

// returns the index of the next free block
int get_next_free_block(int *bit_array) {
	for (int i = 0; i < NUM_BLOCKS; i++)
		if (getBit(bit_array, i) == 1)
			return i;
	return -1;
}

// returns the index of the next free file descriptor
int get_next_free_fd() {
	for (int i = 0; i < MAX_OPEN_FILES; i++)
		if (ofdt->entries[i].inode_no == -1)
			return i;
	return -1;
}

// returns the index of the next free inode position (both file and entry)
int get_next_free_dir() {
	for (int i = 0; i < MAX_INODES; i++)
		if (dir->files[i].size == -1)
			return i;
	return -1;
}

/* 
 * Dir, FBM, and WM are cached for easy and quick access.
 * Should be written to disk after changes.
 */
// void write_dir_to_disk() {
// 	block_t *buf = (block_t*)calloc(dir->req_blocks, BLOCK_SIZE);
// 	memcpy(buf, dir, dir->req_blocks*BLOCK_SIZE);
// 	write_blocks(1, dir->req_blocks, buf); // superblock is in block 0
// 	free(buf);
// }

// void write_fbm_to_disk() {
// 	block_t *buf = (block_t*)calloc(1, BLOCK_SIZE);
// 	memcpy(buf, FBM, BLOCK_SIZE);
// 	write_blocks(NUM_BLOCKS-2, 1, buf);
// 	free(buf);
// }

// void write_wm_to_disk() {
// 	block_t *buf = (block_t*)calloc(1, BLOCK_SIZE);
// 	memcpy(buf, WM, BLOCK_SIZE);
// 	write_blocks(NUM_BLOCKS-1, 1, buf);
// 	free(buf);
// }

// return the amount of full blocks that correspond to the given size in bytes
int bytes_to_blocks_rnd_up(int bytes) {
	// size = 1024 bytes -> 1 block  full
	// size = 1111 bytes -> 2 blocks full
	// size = 2048 bytes -> 2 blocks full

	if (bytes % BLOCK_SIZE == 0) 	
		return (int)((double)bytes / (double)BLOCK_SIZE);
	else 			
		return (int)((double)bytes / (double)BLOCK_SIZE) + 1;
}

int bytes_to_blocks_rnd_down(int bytes) {
	return (int)((double)bytes / (double)BLOCK_SIZE);
}