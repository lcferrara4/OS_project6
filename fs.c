
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
int * BITMAP; 
int * BLOCK_BITMAP; 
int * INODE_BITMAP; 
int * INUMBERS;
int NBLOCKS; 
int CURR_BLOCK; 
int NEXT_AVAILABLE =1; 


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

void inode_load(int inumber, struct fs_inode *inode); 
void inode_save(int inumber, struct fs_inode *inode); 
int determine_block(int inumber, int offset); 

int fs_format(){

    union fs_block block; 
    int i, j; 

    if(MOUNTED){
        fprintf(stderr, "File system already mounted\n"); 
        return 0; 
    }
    NBLOCKS = disk_size(); 
    int blocks = NBLOCKS; 
    int inode_blocks = (.9 + (.1 * blocks)); 
    int k; 

    //update super block
    disk_read(0, block.data); 
    block.super.ninodeblocks = inode_blocks;
    block.super.magic = FS_MAGIC; 
    block.super.nblocks = blocks; 
    disk_write(0, block.data); 
    
    //clear inodes 
    for(i=0; i<blocks-1; i++){
        disk_read(i+1, block.data);  
        for(j=0; j<INODES_PER_BLOCK; j++){
            block.inode[j].isvalid = 0;
            block.inode[j].size = 0; 
            block.inode[j].indirect = 0; 
            for(k=0; k<5; k++){
                block.inode[j].direct[k] = 0; 
            }
        }
        disk_write(i+1, block.data);
    }

	return 1;
}

void fs_debug()
{
	union fs_block block; 
    int i, j, k, x;    
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
        disk_read(i+1, block.data); 
        for(j=0; j<INODES_PER_BLOCK; j++){
            if(block.inode[j].isvalid == 1){
                printf("inode: %d\n", INUMBERS[j]); 
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
    int i, j, inuse; 

    disk_read(0, block.data); //read superblock 
    if(block.super.magic != FS_MAGIC){
        return 0; 
    }

    int inode_blocks = block.super.ninodeblocks; 

    BITMAP = (int *)malloc(sizeof(int)*NBLOCKS); 
    BLOCK_BITMAP = (int *)malloc(sizeof(int)*NBLOCKS); 
    INODE_BITMAP = (int *)malloc(sizeof(int)*NBLOCKS*INODES_PER_BLOCK); 

    BITMAP[0] = 1; //superblock 

    for(i=1; i<inode_blocks; i++){
        inuse = 0; 
        disk_read(i, block.data); 
        for(j=0; j<INODES_PER_BLOCK; j++){
            if (block.inode[j].isvalid == 1){
               inuse += 1;
            }
        }
        if (inuse == INODES_PER_BLOCK){
            BITMAP[i] = 1; 
        } else{
            BITMAP[i] = 0; 
        }
    }
    
    MOUNTED = 1; 
	return 1;
}

int fs_create()
{
	int i; 
    union fs_block block; 
    int free_block = -1; 
    int inumber = -1; 

    for(i=1; i<NBLOCKS; i++){
        if(BITMAP[i] == 0){
            free_block = i; 
            break; 
        }
    }
    if (free_block == -1){
        fprintf(stderr, "No free blocks\n"); 
        return 0; 
    }
    
    disk_read(free_block, block.data);

    for(i=1; i<INODES_PER_BLOCK; i++){
        if(block.inode[i].isvalid == 0){
            inumber = NEXT_AVAILABLE; 
		BLOCK_BITMAP[NEXT_AVAILABLE] = free_block;
		INODE_BITMAP[NEXT_AVAILABLE] = i;
		INUMBERS[i] = NEXT_AVAILABLE;
		NEXT_AVAILABLE++;

            block.inode[i].isvalid = 1; 
            disk_write(free_block, block.data); 
            break; 
        }
    }
    if (inumber == -1){
        fprintf(stderr, "no valid inodes\n"); 
        return 0; 
    }
    if(inumber == INODES_PER_BLOCK -1){
        BITMAP[free_block] = 1; 
    }
    return inumber; 
}

int fs_delete( int inumber )
{

    if(!MOUNTED){
        fprintf(stderr, "File system not mounted\n"); 
        return -1; 
    }
 
    struct fs_inode  curr; 
    int i;  
    
    inode_load(inumber, &curr); 
    
    if (curr.isvalid ==0){
        fprintf(stderr, "Error in deleting inode: does not exist\n"); 
        return 0; 
    }

    curr.isvalid = 0; 
    curr.size = 0; 
    curr.indirect = 0; 
    for(i=0; i<5; i++){
        curr.direct[i] = 0; 
    }
    inode_save(inumber, &curr); 
    
    BITMAP[CURR_BLOCK]=0; 
	INODE_BITMAP[inumber] = -1;
	BLOCK_BITMAP[inumber] = -1;

    return 1;
}

int fs_getsize( int inumber )
{
    if(!MOUNTED){
        fprintf(stderr, "File system not mounted\n"); 
        return -1; 
    }
    
    struct fs_inode curr; 
    inode_load(inumber, &curr); 

    if (curr.isvalid == 0){
        fprintf(stderr, "Error: inode does not exist\n"); 
	    return -1;
    } 

    return curr.size; 
}

int fs_read( int inumber, char *data, int length, int offset )
{
	
    if(!MOUNTED){
        fprintf(stderr, "File system not mounted\n"); 
        return 0; 
    }
    
    union fs_block block; 
    struct fs_inode curr; 
    inode_load(inumber, &curr); 

    if(curr.isvalid == 0){
        fprintf(stderr, "Inode does not exist\n"); 
        return 0; 
    }
    
    if (offset >= curr.size){
	// stops reading from shell
        return 0; 
    }
    if(curr.size == 0){
        fprintf(stderr, "Inode size 0, no data to read\n"); 
        return 0; 
    }

    int block_pointer = determine_block(inumber, offset); 
    disk_read(curr.direct[1], block.data); 
    int j = offset; 
    int bytes_read = 0; 
	int bytes_read_curr = 0;

    while(bytes_read < length){
        if(block_pointer >= 5){
            disk_read(curr.indirect, block.data); 
        }else {
            disk_read(curr.direct[block_pointer], block.data); 
        }	
        if ( bytes_read + DISK_BLOCK_SIZE > length ){
        	memcpy(data + j, block.data, length - bytes_read);
		bytes_read_curr = length - bytes_read;
        	bytes_read = length; 
		if( curr.size < bytes_read )
			return curr.size;
		return bytes_read;
	} else{
        	memcpy(data + j, block.data, DISK_BLOCK_SIZE);
		bytes_read_curr = strlen(block.data);
        	bytes_read += bytes_read_curr; 
	}
	j = bytes_read;
	
	if( bytes_read_curr < DISK_BLOCK_SIZE ){
		printf("Stopped here\n");
		if( curr.size < bytes_read )
			return curr.size;
		return bytes_read;
	}
	
	block_pointer++; 
    }
    
    return bytes_read; 
}

int fs_write( int inumber, const char *data, int length, int offset )
{
    struct fs_inode curr; 
    union fs_block block; 
	inode_load(inumber, &curr); 

    if(curr.isvalid == 0){
        fprintf(stderr, "Inode does not exist\n"); 
        return 0; 
    }

    if(!MOUNTED){
        fprintf(stderr, "File system not mounted\n"); 
        return 0; 
    }
    
    int block_pointer = determine_block(inumber, offset); 
    if(block_pointer == -1){
        fprintf(stderr, "Offset too large\n"); 
        return 0; 
    } 

    
    if(block_pointer == 5){
        curr.indirect = NEXT_AVAILABLE; 
    } else {
        curr.direct[block_pointer] = NEXT_AVAILABLE; 
    }

    NEXT_AVAILABLE++; 

    int bytes_written = 0; 
    int j=offset; 
    while(bytes_written < length){
	if(block_pointer == 5){
		disk_read(curr.indirect, block.data); 
	}else {
		disk_read(curr.direct[block_pointer], block.data); 
	}
	
	if ( strlen(data) - offset > DISK_BLOCK_SIZE ){
		if ( bytes_written + DISK_BLOCK_SIZE > length ){
			memcpy(block.data, data + j, length - bytes_written);
			bytes_written = length;
		} else{
			memcpy(block.data, data + j, DISK_BLOCK_SIZE); 
			bytes_written += DISK_BLOCK_SIZE;
		}
	} else{
		if ( bytes_written + strlen(data) - j > length ){
			memcpy(block.data, data + j, length - bytes_written);
			bytes_written = length;
		} else{
			memcpy(block.data, data + j, strlen(data) - j); 
			bytes_written += strlen(data) - j;
		}
	}
	j = bytes_written; 
	//update inode size
	if(block_pointer == 5){
		disk_write(curr.indirect, block.data); 
	}else {
		disk_write(curr.direct[block_pointer], block.data); 
	}
	if(bytes_written == length){
		curr.size = bytes_written;
		inode_save(inumber, &curr);
		return bytes_written; 
	} 
        
	block_pointer++; 
        if(block_pointer >= 5){
            curr.indirect = NEXT_AVAILABLE; 
        } else {
            curr.direct[block_pointer] = NEXT_AVAILABLE; 
        }
        NEXT_AVAILABLE++;
    }
    return bytes_written; 
}

void inode_load(int inumber, struct fs_inode * fs){
    
    union fs_block block; 
    CURR_BLOCK = BLOCK_BITMAP[inumber];
    int i = INODE_BITMAP[inumber];

    disk_read(CURR_BLOCK, block.data); 
    *fs = block.inode[i]; 
}

void inode_save(int inumber, struct fs_inode * fs){
	
    CURR_BLOCK = BLOCK_BITMAP[inumber];
    int i = INODE_BITMAP[inumber];
    union fs_block block; 

    disk_read(CURR_BLOCK, block.data); 
    block.inode[i] = *fs; 
    disk_write(CURR_BLOCK, block.data); 
}

int determine_block(int inumber, int offset){

    int block_num; 
    if(offset >= 4214784){
        return  -1; 
    }

    if(offset % 4096 == 0){
            block_num = offset / 4096; 
    } else{
        block_num = offset / 4096 + 1;
    }
    
    if(block_num >4){
        block_num =5; 
    }

    return block_num; 
}
