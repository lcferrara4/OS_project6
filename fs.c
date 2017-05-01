
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024



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
	return 0;
}

void fs_debug()
{
	union fs_block block; 
    union fs_block indirect; 
    int i, j, k, x, y;    
    int first = 1;

	disk_read(0,block.data);


	printf("superblock:\n");
    /*if (block.super.magic = FS_MAGIC){
        printf("    Magic number is valid\n"); 
    } else{
        fprintf(stderr, "Invalid magic number\n"); 
        exit(1); 
    }*/
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);


    for (i =0; i<block.super.ninodeblocks; i++){
        disk_read(i+1, block.data); 
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
                    
                    //TO FIX (not sure how to read indirect block yet)
                    disk_read(block.inode[j].indirect, indirect.data); 
                    for(x = 0; x < INODES_PER_BLOCK; x++){
                        for(y=0; k<5; y++){
                            if(indirect.inode[x].direct[y] != 0){
                                printf("%d\n", indirect.inode[x].direct[y]); 
                            }
                        } //end for (y)
                    } //end for (x)
                } // end if
             } //end if 
        } // end for (j)
    } // end for (i)

}

int fs_mount()
{
	return 0;
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
