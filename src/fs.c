/**
 * @file fs.c
 * @author ABDELMOUMENE Djahid 
 * @author AYAD Ishak
 * @brief main filesystem utilities
 * @details initializing the partition files and utility functions
 *  to interact with the os
 */
#include <devutils.h>
#include <fs.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/**
 * @brief format the superblock into the virtual filesystem
 * @details format and calculate the positions and sizes of each section
 * of the filesystem (eg. bitmaps and inode and data blocks)
 * @return this functions returns -1 in case of error and 0 on success
 */
int fs_format_super(struct fs_filesyst fs) {
	struct fs_super_block super;
	/* initializing the log values */
	super.magic = FS_MAGIC;
	super.nreads = 0;
	super.nwrites = 0;
	super.mounts = 1;
	
	super.mtime = get_cur_time();
	super.wtime = super.mtime;
	
	/* calculating the positions of sections */
	/* inode count is prefixed with a ratio, it cannot be null*/
	uint32_t nblocks = fs.nblocks - 1; /* for the super block */
	super.inode_count = SET_MINMAX(nblocks*FS_INODE_RATIO, 1, FS_MAX_INODE_COUNT);
	
	/* calculating the number of bits needed to rep inode count */
	uint32_t bits_to_rep = FS_INODES_PER_BLOCK*super.inode_count;
	uint32_t bits_per_block = FS_BLOCK_SIZE*BITS_PER_BYTE;
	super.inode_bitmap_size = NOT_NULL(bits_to_rep/bits_per_block);
	
	/* the rest is for data and data bitmap */
	uint32_t blocks_left = nblocks - (super.inode_count + super.inode_bitmap_size);
	super.data_bitmap_size = NOT_NULL((int) log2(blocks_left / (FS_BLOCK_SIZE*BITS_PER_BYTE))); /* approximation */

	super.data_count = NOT_NULL(blocks_left - super.data_bitmap_size);
	
	/* sanity check */
	/* all are positive */
	if(	 !(super.inode_count > 0 &&
		   super.inode_bitmap_size > 0 &&
		   super.data_bitmap_size > 0 &&
		   super.data_count > 0))
	{
		fprintf(stderr, "fs_format_super: null size\n");
		return FUNC_ERROR;
	}
	
	/* total is equal to nblocks */
	if(   !(1 +
		   super.inode_count +
		   super.data_count +
		   super.data_bitmap_size +
		   super.inode_bitmap_size == fs.nblocks)) 
	{
		fprintf(stderr, "fs_format_super: wrong total\n");
		return FUNC_ERROR;
	}
	
	/* all blocks are free */
	super.free_data_count = super.data_count;
	super.free_inode_count = super.inode_count * FS_INODES_PER_BLOCK;
	
	/* getting the locations */
	super.inode_bitmap_loc = 1; /* directly after the superblock */
	super.data_bitmap_loc = 1 + super.inode_bitmap_size;
	super.inode_loc = super.data_bitmap_loc + super.data_bitmap_size;
	super.data_loc = super.inode_loc + super.inode_count;
	
	if(fs_write_block(fs, 0, &super, sizeof(super)) < 0) {
		fprintf(stderr, "fs_format_super: cannot write super!\n");
		return FUNC_ERROR;
	}
	return 0;
}

/**
 * @brief dump formatted content of the superblock
 * @details prints a human readable superblock from teh filesystem fs
 */
int fs_dump_super(struct fs_filesyst fs) {
	/* read the super block */
	union fs_block blk;
	if(fs_read_block(fs, 0, &blk) < 0) {
		fprintf(stderr, "fd_dump_super: dump failed, cannot read!\n");
		return FUNC_ERROR;
	}
	struct fs_super_block super = blk.super;
	
	printf("[%d] nblocks: %d\nSuperblock dump:\n", fs.fd, fs.nblocks);
	printf("Magic: %x\n", super.magic);

	printf("Bitmaps: \finode: \f");
	print_range(super.inode_bitmap_loc, super.inode_bitmap_size);
	
	printf("         data: \f");
	print_range(super.data_bitmap_loc, super.data_bitmap_size);
	
	printf("Inode table:\f");
	print_range(super.inode_loc, super.inode_count);
	
	printf("Data blocks:\f");
	print_range(super.data_loc, super.data_count);
	
	printf("Log:\n");
	printf("    number of reads: %d\n", super.nreads);
	printf("    number of writes: %d\n", super.nwrites);
	printf("    number of mounts: %d\n", super.mounts);
	printf("    number of free inodes spaces: %d\n", super.free_inode_count);
	printf("    number of free data spaces: %d\n", super.free_data_count);
	
	printf("    last mount time: %s", timetostr(super.mtime));
	printf("    last write time: %s", timetostr(super.wtime));
	
	return 0;
}

/**
 * @brief format the filesystem
 * @details formats the superblock and sets the bitmaps to 0
 */
int fs_format(struct fs_filesyst fs) {
	/* format the superblock */
	if(fs_format_super(fs) < 0) {
		fprintf(stderr, "fs_format: fs_format_super\n");
		return FUNC_ERROR;
	}
	
	/* set the bitmaps to 0 */
	union fs_block blk;
	
	/* read the super block*/
	struct fs_super_block super;
	if(fs_read_block(fs, 0, &blk) < 0) {
		fprintf(stderr, "fs_format: fs_read_block\n");
		return FUNC_ERROR;
	}
	super = blk.super;
	
	/* reset the blk buffer to 0 */
	memset(&blk, 0, FS_BLOCK_SIZE);
	
	/* set the data bitmap to 0 */
	for(int i=super.data_bitmap_loc; i<super.data_bitmap_loc+super.data_bitmap_size; i++) {
		if(fs_write_block(fs, i, &blk, FS_BLOCK_SIZE) < 0) {
			fprintf(stderr, "fs_format: fs_write_block\n");
			return FUNC_ERROR;
		}
	}

	/* set the inode bitmap to 0 */
	for(int i=super.inode_bitmap_loc; i<super.inode_bitmap_loc+super.inode_bitmap_size; i++) {
		if(fs_write_block(fs, i, &blk, FS_BLOCK_SIZE) < 0) {
			fprintf(stderr, "fs_format: fs_write_block\n");
			return FUNC_ERROR;
		}
	}
	
	return 0;
}

/**
 * @brief utility function to check if a data block is allocated
 */
int fs_is_data_allocated(struct fs_filesyst fs, struct fs_super_block super, uint32_t datanum) {
	datanum --;
	uint32_t blkno = datanum / (BITS_PER_BYTE * FS_BLOCK_SIZE) + super.data_bitmap_loc;
	union fs_block blk;
	if(fs_read_block(fs, blkno, &blk)) {
		fprintf(stderr, "fs_free_inode: fs_read_block!\n");
		return FUNC_ERROR;
	}
	uint32_t blkoff = datanum % (BITS_PER_BYTE * FS_BLOCK_SIZE);
	uint32_t byteoff = blkoff % 8;
	uint8_t byte = blk.data[blkoff / 8];
	
	uint8_t mask = 1;
	mask <<= byteoff;

	return (byte & mask) > 0;
}

/**
 * @brief utility function to check if an inode number is allocated
 */
int fs_is_inode_allocated(struct fs_filesyst fs, struct fs_super_block super, uint32_t inodenum) {
	uint32_t blkno = inodenum / (BITS_PER_BYTE * FS_BLOCK_SIZE) + super.inode_bitmap_loc;
	union fs_block blk;
	if(fs_read_block(fs, blkno, &blk)) {
		fprintf(stderr, "fs_free_inode: fs_read_block!\n");
		return FUNC_ERROR;
	}
	uint32_t blkoff = inodenum % (BITS_PER_BYTE * FS_BLOCK_SIZE);
	uint32_t byteoff = blkoff % 8;
	uint8_t byte = blk.data[blkoff / 8];

	uint8_t mask = 1;
	mask <<= byteoff;

	return (byte & mask) > 0;
}

/**
 * @brief allocate an inode
 * @details allocates the first free inode in the inode table
 * @arg fs: the virtual filesystem
 * @arg super: the superblock
 * @arg inodenum: the inode number allocated
 */
int fs_alloc_inode(struct fs_filesyst fs, struct fs_super_block* super, uint32_t *inodenum) {
	if(super->free_inode_count == 0){
		fprintf(stderr, "fs_alloc_inode: no space left!\n");
		return FUNC_ERROR;
	}

	uint32_t start = super->inode_bitmap_loc;
	uint32_t end = super->inode_bitmap_loc + super->inode_bitmap_size;
	if(inodenum == NULL){
		fprintf(stderr, "fs_alloc_inode: invalid inodenum!\n");
		return FUNC_ERROR;		
	}

	if(end <= start) {
		fprintf(stderr, "fs_alloc_inode: invalid bitmap blocks!\n");
		return FUNC_ERROR;
	}
	
	int found = 0;
	uint32_t off; /* offset of the first null bit in the first block containing one */
	uint32_t blknum;
	union fs_block blk;
	/* parse the inode bitmap and look for the first bit in the first block that is free */
	for(blknum=start; blknum<end && found==0; blknum++) {
		/* read the block */
		if(fs_read_block(fs, blknum, &blk) < 0) {
			fprintf(stderr, "fs_alloc_inode: fs_read_block!\n");
			return FUNC_ERROR;
		}
		/* parse the block for a null bit */
		for(int i=0; i<FS_BLOCK_SIZE && found==0; i++) {
			uint8_t byte = blk.data[i];
			if(byte != 255) {
				off = 0;
				uint8_t temp = byte;
				while(temp) { /* to get the first one */
					temp >>= 1;
					off ++;
				}
				/* calculate the marked byte */
				uint8_t marked_byte = 1;
				marked_byte <<= off;
				marked_byte |= byte;
				off += i * 8; /* add the offset of the byte location in the block */
				found = 1;
				/* mark as read */
				blk.data[i] = marked_byte;
				
				/* write to disk */
				if(fs_write_block(fs, blknum, &blk, FS_BLOCK_SIZE) < 0) {
					fprintf(stderr, "fs_alloc_inode: fs_write_block!\n");
					return FUNC_ERROR;
				}
			}
		}
	}
	
	/* real inode offset in blocks from super->inode_loc */
	uint32_t indno = ((blknum - start - 1) * FS_BLOCK_SIZE * BITS_PER_BYTE) + off;
	*inodenum = indno;

	uint32_t blkno = indno / FS_INODES_PER_BLOCK;

	if(!found || blkno >= super->inode_count) {
		fprintf(stderr, "fs_alloc_inode: no space left\n");
		return FUNC_ERROR;
	}
	
	struct fs_inode nilino = {0};
	if(fs_write_inode(fs, *super, indno, &nilino) < 0) {
		fprintf(stderr, "fs_alloc_inode: can't reinit value\n");
		return FUNC_ERROR;
	}
	super->free_inode_count--;

	/* write to disk */
	if(fs_write_block(fs, 0, super, sizeof(*super)) < 0) {
		fprintf(stderr, "fs_alloc_inode: fs_write_block!\n");
		return FUNC_ERROR;
	}
	return 0;
}

/**
 * @brief writes an inode struct into an inode block location
 */
int fs_write_inode(struct fs_filesyst fs, struct fs_super_block super,
				   uint32_t indno, struct fs_inode *inode)
{
	if(inode == NULL || !fs_is_inode_allocated(fs, super, indno)) {
		fprintf(stderr, "fs_read_inode: invalid arguments!\n");
		return FUNC_ERROR;
	}

	uint32_t blkno = indno / FS_INODES_PER_BLOCK;
	/* getting the real block offset from the start */
	blkno += super.inode_loc;
	
	/* offset in the block containing the inode */
	uint8_t indoff = indno % FS_INODES_PER_BLOCK;
	
	//~ /* reading the block containing the inode */
	union fs_block iblk;
	if(fs_read_block(fs, blkno, &iblk) < 0) {
		fprintf(stderr, "fs_alloc_inode: fs_read_block!\n");
		return FUNC_ERROR;
	}
	//~ /* setting the inode in the block */
	iblk.inodes[indoff] = *inode;
	
	/* writing changes to disk */
	if(fs_write_block(fs, blkno, &iblk, FS_BLOCK_SIZE) < 0) {
		fprintf(stderr, "fs_alloc_inode: fs_write_block!\n");
		return FUNC_ERROR;
	}
	
	return 0;
}

/**
 * @brief reads an inode from the inode table and puts its content 
 * in a the inode struct
 */
int fs_read_inode(struct fs_filesyst fs, struct fs_super_block super,
				   uint32_t indno, struct fs_inode *inode)
{
	if(inode == NULL || !fs_is_inode_allocated(fs, super, indno)) {
		fprintf(stderr, "fs_read_inode: invalid arguments!\n");
		return FUNC_ERROR;
	}
	
	uint32_t blkno = indno / FS_INODES_PER_BLOCK;
	/* getting the real block offset from the start */
	blkno += super.inode_loc;
	
	/* offset in the block containing the inode */
	uint8_t indoff = indno % FS_INODES_PER_BLOCK;
	
	/* reading the block containing the inode */
	union fs_block iblk;
	if(fs_read_block(fs, blkno, &iblk) < 0) {
		fprintf(stderr, "fs_alloc_inode: fs_read_block!\n");
		return FUNC_ERROR;
	}
	/* setting the inode in the block */
	*inode = iblk.inodes[indoff];
	
	return 0;
}

/**
 * @brief dump (print) the content of the inode 
 */
int fs_dump_inode(struct fs_filesyst fs, struct fs_super_block super, uint32_t inodenum) {
	uint32_t blkno = inodenum / FS_INODES_PER_BLOCK + super.inode_loc;
	uint8_t indoff = inodenum % FS_INODES_PER_BLOCK;
	
	union fs_block blk;
	if(fs_read_block(fs, blkno, &blk) < 0) {
		fprintf(stderr, "fd_dump_super: dump failed, cannot read!\n");
		return FUNC_ERROR;
	}
	
	struct fs_inode ind;
	ind = blk.inodes[indoff];
	
	printf("Inode %d dump:\n", inodenum);
	printf("mode: %d\n", ind.mode);
	printf("uid: %d\n", ind.uid);
	printf("gid: %d\n", ind.gid);
	printf("atime: %s", timetostr(ind.atime));
	printf("mtime: %s", timetostr(ind.mtime));
	printf("size: %d\n", ind.size);
	return 0;
}

/**
 * @brief allocate multiple data blocks from the data section
 * @param data      the array of data block pointers (numbers)
 * @param size      the number of blocks to allocate
 */
int fs_alloc_data(struct fs_filesyst fs, struct fs_super_block* super, uint32_t data[], size_t size) {
	/* todo: remove super from argument (read from file directly) and write at the end */
	if(super->free_data_count < size) {
		fprintf(stderr, "fs_alloc_data: no space left!\n");
		return FUNC_ERROR;
	}
	/* param check */
	if(data == NULL || size <= 0) {
		fprintf(stderr, "fs_alloc_data: invalid arguments!\n");
		return FUNC_ERROR;
	}

	uint32_t start = super->data_bitmap_loc;
	uint32_t end = super->data_bitmap_loc + super->data_bitmap_size;
	if(end <= start) {
		fprintf(stderr, "fs_alloc_data: invalid bitmap blocks!\n");
		return FUNC_ERROR;
	}
	
	int left = size;
	uint32_t off; /* offset of the first null bit in the first block containing one */
	uint32_t blknum;
	union fs_block blk;
	/* parse the data bitmap and look for the first bit in the first block that is free */
	for(blknum=start; blknum<end && left>0; blknum++) {
		/* read the block */
		if(fs_read_block(fs, blknum, &blk) < 0) {
			fprintf(stderr, "fs_alloc_data: fs_read_block!\n");
			return FUNC_ERROR;
		}
		/* parse the block for a null bit */
		for(int i=0; i<FS_BLOCK_SIZE && left>0; i++) {
			uint8_t byte = blk.data[i];
			if(byte != 255) {
				off = 0;
				uint8_t temp = byte;
				while(temp) { /* to get the first one */
					temp >>= 1;
					off ++;
				}
				/* calculate the marked byte */
				uint8_t marked_byte = 1;
				marked_byte <<= off;
				marked_byte |= byte;
				off += i * 8; /* add the offset of the byte location in the block */
				/* mark as read */
				blk.data[i] = marked_byte;

				left --;
				data[left] = ((blknum - start) * FS_BLOCK_SIZE * BITS_PER_BYTE) + off + 1;
				/* todo: add test to check if we surpassed the capacity */
				
				/* write to disk */
				if(fs_write_block(fs, blknum, &blk, FS_BLOCK_SIZE) < 0) {
					fprintf(stderr, "fs_alloc_data: fs_write_block!\n");
					return FUNC_ERROR;
				}
				if(byte != 255 && left != 0) {
					i--;
				}
			}
		}
	}
	super->free_data_count -= size;

	/* write to disk */
	if(fs_write_block(fs, 0, super, sizeof(*super)) < 0) {
		fprintf(stderr, "fs_alloc_data: fs_write_block!\n");
		return FUNC_ERROR;
	}
	return 0;
}

/**
 * @brief free an inode from the inode bitmap
 */
int fs_free_inode(struct fs_filesyst fs, struct fs_super_block* super, uint32_t inodenum){
	uint32_t blkno = inodenum / (FS_INODES_PER_BLOCK * FS_BLOCK_SIZE) + super->inode_bitmap_loc;
	union fs_block blk;
	if(fs_read_block(fs, blkno, &blk)) {
		fprintf(stderr, "fs_free_inode: fs_read_block!\n");
		return FUNC_ERROR;
	}
	uint32_t blkoff = inodenum % (BITS_PER_BYTE * FS_BLOCK_SIZE);
	uint32_t byteoff = blkoff % 8;

	uint8_t byte = blk.data[blkoff/8];
	uint8_t unmarked_byte = 1;
	unmarked_byte <<=  byteoff;
	unmarked_byte = ~unmarked_byte;
	unmarked_byte &= byte;
	
	blk.data[blkoff/8] = unmarked_byte;
	super->free_inode_count++;
	
	/* write to disk */
	if(fs_write_block(fs, blkno, &blk, FS_BLOCK_SIZE) < 0) {
		fprintf(stderr, "fs_free_inode: fs_write_block!\n");
		return FUNC_ERROR;
	}
	if(fs_write_block(fs, 0, super, sizeof(*super)) < 0) {
		fprintf(stderr, "fs_free_inode: fs_write_block!\n");
		return FUNC_ERROR;	
	}
	return 0;
}

/**
 * @brief free a data block from the data bitmap
 */
int fs_free_data(struct fs_filesyst fs, struct fs_super_block* super, uint32_t datanum) {
	datanum --;
	uint32_t blkno = datanum / (BITS_PER_BYTE * FS_BLOCK_SIZE) + super->data_bitmap_loc;
	union fs_block blk;
	if(fs_read_block(fs, blkno, &blk)) {
		fprintf(stderr, "fs_free_inode: fs_read_block!\n");
		return FUNC_ERROR;
	}
	uint32_t blkoff = datanum % (BITS_PER_BYTE * FS_BLOCK_SIZE);
	uint32_t byteoff = blkoff % 8;

	uint8_t byte = blk.data[blkoff/8];
	uint8_t unmarked_byte = 1;
	unmarked_byte <<= byteoff;
	unmarked_byte = ~unmarked_byte;
	unmarked_byte &= byte;
	
	blk.data[blkoff/8] = unmarked_byte;
	super->free_data_count++;
	
	/* write to disk */
	if(fs_write_block(fs, blkno, &blk, FS_BLOCK_SIZE) < 0) {
		fprintf(stderr, "fs_free_inode: fs_write_block!\n");
		return FUNC_ERROR;
	}
	if(fs_write_block(fs, 0, super, sizeof(*super)) < 0) {
		fprintf(stderr, "fs_free_inode: fs_write_block!\n");
		return FUNC_ERROR;	
	}
	return 0;
}

/**
 * @brief write multiple data blocks into the data section
 * @param blknums   the array of data block pointers (numbers)
 * @param size      the number of blocks to write
 * @param data      the array of data to write
 */
int fs_write_data(struct fs_filesyst fs, struct fs_super_block super,
				  union fs_block *data, uint32_t *blknums, size_t size)
{
	if(data == NULL || blknums == NULL) {
		fprintf(stderr, "fs_write_data: invalid arguments!\n");
		return FUNC_ERROR;
	}
	
	for(int i=0; i<size; i++) {
		uint32_t blknum = blknums[i] - 1 + super.data_loc;
		union fs_block* d = data + i;
		//~ printf("%d\n", blknums[i]);
		if(fs_write_block(fs, blknum, d, FS_BLOCK_SIZE) < 0) {
			fprintf(stderr, "fs_write_data: fs_write_block\n");
			return FUNC_ERROR;
		}
	}
	return 0;
}

/**
 * @brief read multiple data blocks into the data section
 * @param blknums   the array of data block pointers (numbers)
 * @param size      the number of blocks to read
 * @param data      the array of data to read
 */
int fs_read_data(struct fs_filesyst fs, struct fs_super_block super,
				 union fs_block *data, uint32_t *blknums, size_t size)
{
	if(data == NULL || blknums == NULL) {
		fprintf(stderr, "fs_read_data: invalid arguments!\n");
		return FUNC_ERROR;
	}
	
	for(int i=0; i<size; i++) {
		uint32_t blknum = blknums[i] - 1 + super.data_loc;
		union fs_block* d = data + i;
		if(fs_read_block(fs, blknum, d) < 0) {
			fprintf(stderr, "fs_read_data: fs_read_block\n");
			return FUNC_ERROR;
		}
	}
	return 0;
}
