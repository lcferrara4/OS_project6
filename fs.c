
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

int MOUNTED = 0; 

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int fs_format()
{

    union fs_block block; 
    char super_data[DISK_BLOCK_SIZE]; 
    int i, j; 

    if(MOUNTED){
        fprintf(stderr, "File system already mounted\n"); 
        return 0; 
    }
    int blocks = disk_size(); 
 
    
    int inode_blocks = (.9 + (.1 * blocks));  


    disk_read(0, block.data); 
    printf("after read\n"); 
    block.super.ninodeblocks = inode_blocks;
    block.super.magic = FS_MAGIC; 
    block.super.nblocks = blocks; 
    disk_write(0, block.data); 

    printf("blocks: %d\n inode_blocks: %d\n", block.super.nblocks, block.super.ninodeblocks); 

    printf("after reset inode blocks\n"); 
    for(i=0; i<blocks-1; i++){
        disk_read(i+1, block.data);  
        for(j=0; j<INODES_PER_BLOCK; j++){
            block.inode[j].isvalid = 0; 
        }
        disk_write(i+1, block.data);
    }
   	return 1;
}

void fs_debug()
{
	union fs_block block; 
    int i, j, k, x, y;    
    int first = 1;

	disk_read(0,block.data);

    if(block.super.magic != FS_MAGIC){
        fprintf(stderr, "Magic number not valid\n"); 
        exit(1); 
    } 

	printf("superblock:\n");
  
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

    int inode_blocks = block.super.ninodeblocks; 

    for (i =0; i<inode_blocks; i++){
        disk_read(i, block.data); 
        for(j=0; j<INODES_PER_BLOCK; j++){
            if(block.inode[j].isvalid == 1){
                printf("inode: %d\n", j); 
                printf("    size: %d bytes\n", block.inode[j].size); 
                for(k=0; k<5; k++){
                    if (block.inode[j].direct[k] != 0){
                        if (first){
                            printf("    direct blocks: "); 
                            first = 0; 
                        }
                        printf("%d ", block.inode[j].direct[k]); 
                    } // end if 
                } // end for (k)
                first = 1; 
                printf("\n");
                if (block.inode[j].indirect != 0){
                    printf("    indirect block: %d\n", block.inode[j].indirect);
                    
                    union fs_block indirect_info; 
                    printf("    indirect data blocks: "); 
                    
                    disk_read(block.inode[j].indirect, indirect_info.data); 
                    
                    for(x = 0; x < POINTERS_PER_BLOCK; x++){
                        if (indirect_info.pointers[x] != 0 ){
                            printf("%d ", indirect_info.pointers[x]); 
                        }
                    } 
                    printf("\n"); //end for (x)
                } // end if
             } //end if 
        } // end for (j)
    } // end for (i)

}

int fs_mount()
{
    union fs_block block; 
    int i, j; 

    disk_read(0, block.data); //read superblock 

    if(block.super.magic != FS_MAGIC){
        return 0; 
    }

    int inode_blocks = block.super.ninodeblocks; 
    
    for(i=0; i<inode_blocks; i++){
        disk_read(i, block.data); 
        for(j=0; j<INODES_PER_BLOCK; j++){
            if (block.inode[j].isvalid == 1){
            }
        }
    }

    
    MOUNTED = 1; 
	return 1;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
