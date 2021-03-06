/**
 * @file test2.c
 * @author ABDELMOUMENE Djahid 
 * @author AYAD Ishak
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <fs.h>
#include <disk.h>
#include <devutils.h>

/**
 * @author ABDELMOUMENE Djahid 
 * @author AYAD Ishak
 * @brief program to test filesystem functions
 */
int main(int argc, char** argv) {
	char cwd[512] = "./bin/partition";
	struct fs_filesyst fs;
	printf("Creating filesyst..\n");
	creatfile(cwd, 100000, &fs);

	printf("formatting filesystem..\n");
	fs_format(fs);
	
	printf("dumping superblock\n");
	fs_dump_super(fs);
	
	/* read the super block*/
	union fs_block blk;
	struct fs_super_block super;
	if(fs_read_block(fs, 0, &blk) < 0) {
		fprintf(stderr, "fs_format: fs_read_block\n");
		return FUNC_ERROR;
	}
	super = blk.super;
	
	uint32_t no;
	struct fs_inode ind;
	memset(&ind, 0, sizeof(ind));
	ind.mode = 123;
	ind.uid = 123;
	ind.gid = 123;
	ind.atime = 123;
	ind.mtime = 123;	
	
	for(int i=0; i<60; i++) {
		printf("\n[%d]ALLOCATING\n", i);
		fs_alloc_inode(fs, &super, &no);
		fs_write_inode(fs, super,no, &ind);
		fs_dump_inode(fs, super, no);
	}

	fs_free_inode(fs, &super, 0);
	
	uint32_t data[20];
	
	union fs_block b[20];
	memset(&b, 0, sizeof(b));

	fs_alloc_data(fs, &super, data, 20);
	fs_write_data(fs, super, b, data, 20);
	for(int i=0; i<20; i++) {
		printf("allocated %u\n", data[i]);
		
	}
	fs_read_data(fs, super, b, data, 20);

	fs_dump_super(fs);
	
	disk_close(&fs);
	return 0;
}
