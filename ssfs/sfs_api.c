#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 	// dup
#include "sfs_api.h"
#include "disk_emu.h"

#define BLOCK_SIZE			1024
#define BLOCK_SIZE_NULL_T	1025	// null terminated block
#define NUM_BLOCKS			1024
#define NUM_DIRECT_BLOCKS	14
#define MAX_OPEN_FILES		200
#define MAX_INODES			72 	// 1021 available blocks / 14 blocks per i-node
								// = 72.9 ~ 72 files

typedef struct _inode_t {
	// ints are 32 bits long
	int size;
	int direct[NUM_DIRECT_BLOCKS];
	int indirect;
} inode_t;

typedef struct _superblock_t {
	unsigned char magic[4]; // unsigned char is 8 bits = 1 byte
	int block_size;
	int file_system_size;
	int dir_block_size;
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
	int size;
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

void mkssfs(int fresh){
	superblock_t *superblock;
	inode_t *jnode;
	inode_t *dir_node;

	if (fresh == 1) {
		if (init_fresh_disk("holodisk", BLOCK_SIZE, NUM_BLOCKS) != 0)
			fprintf(stderr, "Could not create new disk.\n");
		
		// initialize the open file desc table and dir caches
		ofdt = (open_fd_table_t*)calloc(1, sizeof(open_fd_table_t));
		ofdt->full = 0;

		// root node, points to all blocks containing i-nodes (dir->files)
		jnode = (inode_t*)calloc(1, sizeof(inode_t));
		jnode->size = MAX_INODES * sizeof(inode_t); // 72 * 64 = 4608 ~ 5 blocks
		int dir_files_blocks = bytes_to_blocks_rnd_up(jnode->size);

		// the first i-node points to all the blocks containing the directory itself (dir->entries)
		dir_node = (inode_t*)calloc(1, sizeof(inode_t));
		dir_node->size = MAX_INODES * sizeof(dir_entry_t); // 72 * 16 = 1152 ~ 2 blocks
		int dir_entries_blocks = bytes_to_blocks_rnd_up(dir_node->size);

		dir = (directory_t*)calloc(1, (dir_entries_blocks + dir_files_blocks)*BLOCK_SIZE);
		dir->size = (dir_entries_blocks + dir_files_blocks) * BLOCK_SIZE;
		dir->full = 0;

		// initialize ofdt and dir to empty entries
		for (int i = 0; i < MAX_OPEN_FILES; i++) {
			ofdt->entries[i].inode_no = -1;
			ofdt->entries[i].read_ptr = -1;
			ofdt->entries[i].write_ptr = -1;

			ofdt->entries[i].inode.size = -1;
			ofdt->entries[i].inode.indirect = -1;
			for (int j = 0; j < NUM_DIRECT_BLOCKS; j++)
				ofdt->entries[i].inode.direct[j] = -1;
		}

		for (int i = 0; i < MAX_INODES; i++) {
			dir->entries[i].inode_no = -1;
			dir->entries[i].filename[0] = '\0';

			dir->files[i].size = -1;
			dir->files[i].indirect = -1;
			for (int j = 0; j < NUM_DIRECT_BLOCKS; j++)
				dir->files[i].direct[j] = -1;
		}

		// initialize FBM, WM to 1's (all empty, all writable)
		FBM = (bit_array_t*)calloc(1, BLOCK_SIZE);
		WM  = (bit_array_t*)calloc(1, BLOCK_SIZE);
		for (int i = 0; i < NUM_BLOCKS; i++) {
			setBit(FBM->four_bytes, i);
			setBit(WM->four_bytes , i);
		}
		clrBit(FBM->four_bytes, NUM_BLOCKS-2);
		clrBit(FBM->four_bytes, NUM_BLOCKS-1);

		// initialize superblock and reserve the first block for it
		// TODO: cached? cannot update # of inodes properly
		superblock = (superblock_t*)calloc(1, BLOCK_SIZE);
		superblock->block_size = BLOCK_SIZE;
		superblock->file_system_size = NUM_BLOCKS * BLOCK_SIZE;
		superblock->no_of_inodes = 0;
		superblock->dir_block_size = dir->size / BLOCK_SIZE;
		superblock->root = *jnode;
		int sb_index = get_next_free_block(FBM->four_bytes); // should be block 0
		clrBit(FBM->four_bytes, sb_index);

		// find free blocks to copy dir to, and reserve them
		int nfb = 0;	
		for (int i = 0; i < dir_files_blocks; i++) {
			nfb = get_next_free_block(FBM->four_bytes);
			jnode->direct[i] = nfb;
			clrBit(FBM->four_bytes, nfb);
		}

		for (int i = 0; i < dir_entries_blocks; i++) {
			nfb = get_next_free_block(FBM->four_bytes);
			dir_node->direct[i] = nfb;
			clrBit(FBM->four_bytes, nfb);
		}

		// add root and dir nodes to dir->entries and dir->files
		dir->files[0] = *jnode;
		dir->entries[0].inode_no = get_next_free_dir();
		strcpy(dir->entries[0].filename, "root.blks");
		superblock->no_of_inodes = ++dir->full;

		dir->files[1] = *dir_node;
		dir->entries[1].inode_no = get_next_free_dir();
		strcpy(dir->entries[1].filename, "dir.blks");
		superblock->no_of_inodes = ++dir->full;

		// copy blocks to buffer, and write to disk
		char *buf = (char*)calloc(1, BLOCK_SIZE_NULL_T);
		buf[BLOCK_SIZE] = '\0';

		memcpy(buf, superblock, BLOCK_SIZE);
		write_blocks(sb_index, 1, buf);
		free(buf);

		// separate functions since these will be accessed often
		write_dir_to_disk();
		write_fbm_to_disk();
		write_wm_to_disk();

		free(jnode);
		free(dir_node);
		free(superblock);

		// TODO: FREE GLOBALS

	} else if (fresh == 0) {
		if(init_disk("holodisk", BLOCK_SIZE, NUM_BLOCKS) != 0)
			fprintf(stderr, "Could not create new disk.\n");

		// initialize the open file desc table
		ofdt = (open_fd_table_t*)calloc(1, sizeof(open_fd_table_t));
		ofdt->full = 0;
		
		for (int i = 0; i < MAX_OPEN_FILES; i++) {
			ofdt->entries[i].inode_no = -1;
			ofdt->entries[i].read_ptr = -1;
			ofdt->entries[i].write_ptr = -1;

			ofdt->entries[i].inode.size = -1;
			ofdt->entries[i].inode.indirect = -1;
			for (int j = 0; j < NUM_DIRECT_BLOCKS; j++)
				ofdt->entries[i].inode.direct[j] = -1;
		}

		// read parts of the file system
		char *buf = (char*)calloc(1, BLOCK_SIZE_NULL_T);
		buf[BLOCK_SIZE] = '\0';

		// superblock
		superblock = (superblock_t*)calloc(1, BLOCK_SIZE);
		read_blocks(0, 1, superblock);

		// FBM and WM
		FBM = (bit_array_t*)calloc(1, BLOCK_SIZE);
		WM  = (bit_array_t*)calloc(1, BLOCK_SIZE);
		read_blocks(NUM_BLOCKS-2, 1, FBM);
		read_blocks(NUM_BLOCKS-1, 1, WM);

		// dir
		dir = (directory_t*)calloc(1, superblock->dir_block_size);
		read_blocks(1, 1 + superblock->dir_block_size, dir);
	}

}

/* 
 * opens a file, or creates it if it does not exist.
 * returns the file's index in the open fd table, or -1 on error.
 */
int ssfs_fopen(char *name){
	// check if file exists; if so, store its index for later
	int file_exists = -1;
	for (int i = 0; i < MAX_INODES; i++)
		if (strcmp(dir->entries[i].filename, name) == 0) {
			file_exists = i;
			break;
		}

	if (dir->full == MAX_INODES && file_exists == -1) {
		fprintf(stderr, "Error: Too many files in the file system (max = %d); cannot create a new file.\n", MAX_INODES);
		return -1;
	}
	if (ofdt->full == MAX_OPEN_FILES) {
		fprintf(stderr, "Error: Too many open files (max = %d)\n", MAX_OPEN_FILES);
		return -1;
	}

	// file does not exist; create it
	if (file_exists == -1) {
		file_exists = get_next_free_dir();
		dir->files[file_exists].size = 0;
		// no blocks associated with file yet -- don't adjust direct[i] pointers

		dir->entries[file_exists].inode_no = file_exists;
		strcpy(dir->entries[file_exists].filename, name);
		dir->full++;

		write_dir_to_disk();
	}

	// add file entry to open fd table
	int fd_index = get_next_free_fd();

	ofdt->entries[fd_index].inode= dir->files[file_exists]; // copy of inode 
	ofdt->entries[fd_index].inode_no = file_exists;
	ofdt->entries[fd_index].read_ptr = 0;
	ofdt->entries[fd_index].write_ptr = dir->files[file_exists].size; // size of 1024: [0, 1023], new data written to 1024 onwards
	ofdt->full++;

    return fd_index;
}

/* 
 * closes a file (removes its entry from the open fd table).
 * returns 0 on success, -1 on error.
 */
int ssfs_fclose(int fileID) {
	if (ofdt->full == 0) {
		fprintf(stderr, "Error: No open file descriptors\n");
		return -1;
	}
	if (fileID < 0 || fileID >= MAX_OPEN_FILES) {
		fprintf(stderr, "Error: File descriptor %d out of bounds [0, %d]\n", fileID, MAX_OPEN_FILES);
		return -1;
	}
	if (ofdt->entries[fileID].inode_no == -1) {
		fprintf(stderr, "Error: No open file associated with file descriptor %d\n", fileID);
		return -1;
	}

	ofdt->entries[fileID].inode_no = -1;
	ofdt->entries[fileID].inode.size = -1;
	ofdt->entries[fileID].read_ptr = -1;
	ofdt->entries[fileID].write_ptr = -1;

	ofdt->full--;
	return 0;
}

/* 
 * move read pointer to given location.
 * returns 0 on success, -1 on error.
 */
int ssfs_frseek(int fileID, int loc) {
	if (ofdt->full == 0) {
		fprintf(stderr, "Error: No open file descriptors\n");
		return -1;
	}
	if (fileID < 0 || fileID > MAX_OPEN_FILES) {
		fprintf(stderr, "Error: File descriptor %d out of bounds [0, %d]\n", fileID, MAX_OPEN_FILES);
		return -1;
	}
	if (ofdt->entries[fileID].inode_no   == -1 || 
		ofdt->entries[fileID].inode.size == -1) {
		fprintf(stderr, "Error: No open file associated with file descriptor %d\n", fileID);
		return -1;
	}
	if (loc < 0 || 
		(loc >= ofdt->entries[fileID].inode.size && loc != 0) || // cannot read at or past the size, due to indexes being [0, size-1] unless size is 0
		loc >= BLOCK_SIZE * NUM_DIRECT_BLOCKS) {
		fprintf(stderr, "Error: Read  pointer cannot be moved to %d; file size is %d\n", loc, ofdt->entries[fileID].inode.size);
		return -1;
	}

	ofdt->entries[fileID].read_ptr = loc;
    return 0;
}

/* 
 * move write pointer to given location.
 * returns 0 on success, -1 on error.
 */
int ssfs_fwseek(int fileID, int loc) {
	if (ofdt->full == 0) {
		fprintf(stderr, "Error: No open file descriptors\n");
		return -1;
	}
	if (fileID < 0 || fileID > MAX_OPEN_FILES) {
		fprintf(stderr, "Error: File descriptor %d out of bounds [0, %d]\n", fileID, MAX_OPEN_FILES);
		return -1;
	}
	if (ofdt->entries[fileID].inode_no   == -1 || 
		ofdt->entries[fileID].inode.size == -1) {
		fprintf(stderr, "Error: No open file associated with file descriptor %d\n", fileID);
		return -1;
	}
	if (loc < 0 || 
		(loc >= ofdt->entries[fileID].inode.size && loc != 0) || // cannot read at or past the size, due to indexes being [0, size-1] unless size is 0
		loc >= BLOCK_SIZE * NUM_DIRECT_BLOCKS) {
		fprintf(stderr, "Error: Write pointer cannot be moved to %d; file size is %d\n", loc, ofdt->entries[fileID].inode.size);
		return -1;
	}

	ofdt->entries[fileID].write_ptr = loc;
    return 0;
}

/* 
 * write the data given in buf into the open file at index fileID.
 * starts writing at the location of the write pointer for that file.
 * returns the number of bytes written, or -1 on error.
 */
int ssfs_fwrite(int fileID, char *buf, int length) {
	if (ofdt->full == 0) {
		fprintf(stderr, "Error: No open file descriptors\n");
		return -1;
	}
	if (fileID < 0 || fileID >= MAX_OPEN_FILES) {
		fprintf(stderr, "Error: File descriptor %d out of bounds [0, %d]\n", fileID, MAX_OPEN_FILES);
		return -1;
	}
	if (ofdt->entries[fileID].inode_no == -1) {
		fprintf(stderr, "Error: No open file associated with file descriptor %d\n", fileID);
		return -1;
	}
	if (length <= 0) {
		fprintf(stderr, "Error: Cannot write less than or 0 bytes\n");
		return -1;
	}

	int ino = ofdt->entries[fileID].inode_no;

	// find the block in which we need to write
	int start_write_in_this_block = bytes_to_blocks_rnd_down(ofdt->entries[fileID].write_ptr); // round down (write inside a full block)

	// find how many blocks the write operation needs to modify
	// even if the size of the write is only 2 blocks, may need to modify 3 depending on how many blocks are overlapped
	int req_blocks = bytes_to_blocks_rnd_up(length + ofdt->entries[fileID].write_ptr); // round up (need n free blocks)

	// TODO: use indirect inode pointer to solve this
	if (req_blocks > NUM_DIRECT_BLOCKS) {
		fprintf(stderr, "Error: File too big; cannot fit in free blocks\n");
		return -1;
	}

	int free_blocks_in_mem = 0;
	for (int i = 0; i < NUM_BLOCKS; i++)
		if (getBit(FBM->four_bytes, i) == 1)
			free_blocks_in_mem++;

	if (req_blocks > free_blocks_in_mem) {
		fprintf(stderr, "Error: Filesystem too full to write file\n");
		return -1;
	}

	int it = 0; // i != iteration, so need a separate counter
	int buf_ptr = 0;
	int size_of_write = 0;
	int end_ptr_loc = ofdt->entries[fileID].write_ptr + length; // TODO: move write_ptr to this after write?
	
	// position to write to, relative to start of the first block (will never be > 1024)
	int wpos_rel = ofdt->entries[fileID].write_ptr - start_write_in_this_block * BLOCK_SIZE;

	char *tmp = (char*)calloc(1, BLOCK_SIZE_NULL_T);
	tmp[BLOCK_SIZE] = '\0';

	for (int i = start_write_in_this_block; i < start_write_in_this_block + req_blocks; i++) {
		int write_loc = dir->files[ino].direct[i];

		// trying to write to empty block. assign a new block to this file for writing
		if (write_loc == -1) {
			int nfb = get_next_free_block(FBM->four_bytes);
			write_loc = nfb;
			dir->files[ino].direct[i] = nfb;
			clrBit(FBM->four_bytes, nfb);
		}

		if (end_ptr_loc - BLOCK_SIZE * it < BLOCK_SIZE)
			size_of_write = end_ptr_loc - BLOCK_SIZE * it - wpos_rel; // wpos_rel will be =0 if not on first iteration
		else
			size_of_write = BLOCK_SIZE - wpos_rel;

		// wpos_rel is not at the start of the block => we are at the first iteration of this loop
		// need to preserve old part of a block and start writing from there
		if ((wpos_rel % BLOCK_SIZE) != 0)
			read_blocks(write_loc, 1, tmp);

		memcpy(tmp + wpos_rel, buf + buf_ptr, size_of_write);
		write_blocks(write_loc, 1, tmp);


		// reset wpos_rel to write from beginning of a block from now on
		if ((wpos_rel % BLOCK_SIZE) != 0)
			wpos_rel = 0;
		
		buf_ptr += size_of_write;
		it++;
	}

	dir->files[ino].size += buf_ptr;
	ofdt->entries[fileID].inode = dir->files[ino];

	write_dir_to_disk();
	write_fbm_to_disk();

	free(tmp);

    return buf_ptr;
}

/* 
 * read the data at the location of the read pointer for the file 
 * at index fileID into buf.
 * returns the number of bytes read, or -1 on error.
 */
int ssfs_fread(int fileID, char *buf, int length) {
	if (ofdt->full == 0) {
		fprintf(stderr, "Error: No open file descriptors\n");
		return -1;
	}
	if (fileID < 0 || fileID >= MAX_OPEN_FILES) {
		fprintf(stderr, "Error: File descriptor %d out of bounds [0, %d]\n", fileID, MAX_OPEN_FILES);
		return -1;
	}
	if (ofdt->entries[fileID].inode_no == -1) {
		fprintf(stderr, "Error: No open file associated with file descriptor %d\n", fileID);
		return -1;
	}
	if (length <= 0) {
		fprintf(stderr, "Error: Cannot read less than or 0 bytes\n");
		return -1;
	}

	int ino = ofdt->entries[fileID].inode_no;

	// find the block in which we need to read
	int start_read_in_this_block = bytes_to_blocks_rnd_down(ofdt->entries[fileID].read_ptr); // round down (read inside a full block)

	// find how many blocks need to be read
	int req_blocks = bytes_to_blocks_rnd_up(length + ofdt->entries[fileID].read_ptr);

	if (req_blocks > NUM_DIRECT_BLOCKS) {
		fprintf(stderr, "Error: Asking to read too many blocks\n");
		return -1;
	}

	int it = 0; // i != iteration, so need a separate counter
	int buf_ptr = 0;
	int size_of_read = 0;
	int end_ptr_loc = ofdt->entries[fileID].read_ptr + length; // TODO: move read_ptr to this after write?
	
	// position to read from, relative to start of block (will never be > 1024)
	int rpos_rel = ofdt->entries[fileID].read_ptr - start_read_in_this_block * BLOCK_SIZE;

	char *tmp = (char*)calloc(1, BLOCK_SIZE_NULL_T);
	tmp[BLOCK_SIZE] = '\0';

	for (int i = start_read_in_this_block; i < start_read_in_this_block + req_blocks; i++) {
		int read_loc = dir->files[ino].direct[i];

		// trying to read from an empty block.
		if (read_loc == -1) {
			fprintf(stderr, "Error: read pointer points to an empty block\n");
			free(tmp);
			return -1;
		}

		read_blocks(read_loc, 1, tmp);

		if (end_ptr_loc - BLOCK_SIZE * it < BLOCK_SIZE)
			size_of_read = end_ptr_loc - BLOCK_SIZE * it - rpos_rel; // rpos_rel will be =0 if not on first iteration
		else
			size_of_read = BLOCK_SIZE - rpos_rel;

		memcpy(buf + buf_ptr, tmp + rpos_rel, size_of_read);
		
		// reset rpos_rel to read from beginning of a block from now on
		if ((rpos_rel % BLOCK_SIZE) != 0)
			rpos_rel = 0;

		buf_ptr += size_of_read;
		it++;
	}

	free(tmp);

    return buf_ptr;
}

/* 
 * removes the file and all associated data:
 * 	- removes entry from the directory
 *	- removes i-node pointing to the file's data blocks
 *	- sets the occupied data blocks to free in the FBM
 * returns 0 on success, -1 on error.
 */
int ssfs_remove(char *file) {
	int file_exists = -1;
	for (int i = 0; i < MAX_INODES; i++)
		if (strcmp(dir->entries[i].filename, file) == 0)
			file_exists = i;

	if (dir->full == 0) {
		fprintf(stderr, "Error: There are no files in the file system\n");
		return -1;
	}
	if (file_exists == -1) {
		fprintf(stderr, "Error: Could not find the file '%s' in the file system\n", file);
		return -1;
	}

	// free blocks associated with inode
	for (int i = 0; i < NUM_DIRECT_BLOCKS; i++)
		setBit(FBM->four_bytes, dir->files[file_exists].direct[i]);

	// mark inode and its entry as free
	dir->files[file_exists].size = -1;
	dir->entries[file_exists].inode_no = -1;
	dir->full--;

	write_dir_to_disk();
	write_fbm_to_disk();

    return 0;
}

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
void write_dir_to_disk() {
	char *buf = (char*)calloc(1, dir->size + 1);
	buf[dir->size] = '\0';
	memcpy(buf, dir, dir->size);
	write_blocks(1, dir->size / BLOCK_SIZE, buf); // dir starts after super block (1 onwards)
	free(buf);
}

void write_fbm_to_disk() {
	char *buf = (char*)calloc(1, BLOCK_SIZE_NULL_T);
	buf[BLOCK_SIZE] = '\0';
	memcpy(buf, FBM, BLOCK_SIZE);
	write_blocks(NUM_BLOCKS-2, 1, buf);
	free(buf);
}

void write_wm_to_disk() {
	char *buf = (char*)calloc(1, BLOCK_SIZE_NULL_T);
	buf[BLOCK_SIZE] = '\0';
	memcpy(buf, WM, BLOCK_SIZE);
	write_blocks(NUM_BLOCKS-1, 1, buf);
	free(buf);
}

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