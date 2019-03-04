#ifndef MEMINIT_H
#define MEMINIT_H
#include <stdint.h>

#define FS_MAGIC 0xF0F03410 /* magic number for our filesystem */
#define FS_BLOCK_SIZE 4098 /* block size in bytes */
#define FS_POINTERS_PER_BLOCK 1024 /* no of pointers (used by inodes) per block in bytes*/
#define FS_INODES_PER_BLOCK 5 /* no of inodes per block */
#define FS_DIRECT_POINTERS_PER_INODE 8 /* no of direct data pointers in each inode */
/**
 * @brief super block structure
 * @details the structure of the super block the first block stored
 * stored in memory contains general information about the filesystem
 * and other useful information, with a total size of 28 bytes.
 */
struct fs_super_block  {
	uint32_t magic; /* the filesystem magic number */
	uint32_t inode_count; /* no of inodes */
	uint32_t block_count; /* no of blocks */
	uint32_t free_inode_count; 
	uint32_t free_block_count;
	
	uint32_t mtime;	/* time of mount of the filesystem */
	uint32_t wtime; /* last write time */
};

/**
 * @brief inode structure
 * @details the structure of inodes contains information about one file
 * with a total size of 26 bytes.
 */
struct fs_inode {
	uint16_t mode; /* file type and permissions */
	uint16_t uid; /* id of owner */
	uint16_t gid; /* group id of owners */
	uint32_t atime; /* last access time in seconds since the epoch */
	uint32_t mtime; /* last modification time in seconds since the epoch*/
	uint32_t size; /* size of the file in bytes */
	uint32_t direct[FS_DIRECT_POINTERS_PER_INODE]; /* direct data blocks */
	uint32_t indirect; /* indirect data blocks */
};


/**
 * @brief union of a block structure
 * @details a block can either be a super block, or and array of inodes
 * or an array of pointers to other blocks, or an array of data  bytes.
 * 
 */
union fs_block {
	struct fs_super_block super; /* super block */
	struct fs_inode inodes[FS_INODES_PER_BLOCK]; /* array of inodes */
	uint32_t pointers[FS_POINTERS_PER_BLOCK]; /* array of pointers */
	uint8_t data[FS_BLOCK_SIZE]; /* array of data bytes */
};

/**
 * @brief virtual filesystem structure
 * @details contains information about the file used to simulate a disk
 * partition
 */
struct fs_filesyst {
	uint32_t fd; /* file descriptor */
	uint32_t tot_size; /* total size of our file (partition) */
};

/* prototypes */
int creatfile(const char* filename, size_t size);

#endif
