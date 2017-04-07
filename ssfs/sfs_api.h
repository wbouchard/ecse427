//Functions you should implement. 
//Return -1 for error besides mkssfs
void mkssfs(int fresh);
int ssfs_fopen(char *name);
int ssfs_fclose(int fileID);
int ssfs_frseek(int fileID, int loc);
int ssfs_fwseek(int fileID, int loc);
int ssfs_fwrite(int fileID, char *buf, int length);
int ssfs_fread(int fileID, char *buf, int length);
int ssfs_remove(char *file);
int ssfs_commit();
int ssfs_restore(int cnum);
int get_next_free_block(int *bit_array);
int get_next_free_fd();
int get_next_free_dir();
void write_dir_to_disk();
void write_fbm_to_disk();
void write_wm_to_disk();
int bytes_to_blocks_rnd_up(int bytes);
int bytes_to_blocks_rnd_down(int bytes);
void setBit(int *bit_array, int i);
void clrBit(int *bit_array, int i);
int  getBit(int *bit_array, int i);