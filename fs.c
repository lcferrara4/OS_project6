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
int * NEXT_AVAILABLE; 

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
int get_NEXT_AVAILABLE();
void release_inumber(int inumber);

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
    
	NBLOCKS = disk_size(); 
	if( NBLOCKS < block.super.nblocks ){
		printf("Not enough disk space to hold this image.\n");
		return;
	}

    printf("superblock:\n");

    printf("    %d blocks\n",block.super.nblocks);
    printf("    %d inode blocks\n",block.super.ninodeblocks);
    printf("    %d inodes\n",block.super.ninodes);

    int inode_blocks = block.super.ninodeblocks; 
	int start_inode = 2;
    for (i =0; i<inode_blocks; i++){
        disk_read(i+1, block.data); 
        for(j=0; j<INODES_PER_BLOCK; j++){
            if(block.inode[j].isvalid == 1){
                if(!MOUNTED){
                    printf("inode: %d\n", start_inode);   
					start_inode++;
                } else{
                    printf("inode: %d\n", INUMBERS[INODES_PER_BLOCK * i + j]); 
                } 
				printf("    size: %d bytes\n", block.inode[j].size); 
                for(k=0; k<POINTERS_PER_INODE; k++){
                    if (block.inode[j].direct[k] != 0){
                        if (first){
                            printf("    direct blocks: "); 
                            first = 0; 
                        }
                        printf("%d ", block.inode[j].direct[k]); 
                    }
                }
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
                    printf("\n");
                }
             }
        }
    }
}

int fs_mount()
{
    union fs_block block; 
    int i, j;

	// Read superblock
    disk_read(0, block.data); 
    if(block.super.magic != FS_MAGIC){
		printf("Error: No superblock set.\n");
        return 0; 
    }
  
	NBLOCKS = disk_size(); 
	if( NBLOCKS < block.super.nblocks ){
		printf("Not enough disk space to hold this image.\n");
		return 0;
	}

	// Set up bitmaps
    BITMAP = (int *)malloc(sizeof(int)*NBLOCKS); 
    BLOCK_BITMAP = (int *)malloc(sizeof(int)*NBLOCKS); 
	NEXT_AVAILABLE = (int *)malloc(sizeof(int)*NBLOCKS);  
	for( i = 0; i < NBLOCKS; i++ ){
		NEXT_AVAILABLE[i] = 0;
		BITMAP[i] = 0;
		BLOCK_BITMAP[i] = 0;
	}

    INODE_BITMAP = (int *)malloc(sizeof(int)*NBLOCKS*INODES_PER_BLOCK); 
    INUMBERS = (int *)malloc(sizeof(int)*NBLOCKS*INODES_PER_BLOCK); 
	for( i = 0; i < NBLOCKS * INODES_PER_BLOCK; i++ ){
		INODE_BITMAP[i] = 0;
		INUMBERS[i] = -1;
	}
 
	BITMAP[0] = 1; //superblock 
	NEXT_AVAILABLE[0] = 1;
	NEXT_AVAILABLE[1] = 1; // inode block 1

	// assign the bitmaps
	int start_inode = 2;
    int inode_blocks = block.super.ninodeblocks; 
    int k, p; 
	for(i = 0; i < inode_blocks; i++){
        for(j=0; j<INODES_PER_BLOCK; j++){
        	disk_read(i+1, block.data);  
			if( block.inode[j].isvalid == 1){
				INUMBERS[i * INODES_PER_BLOCK + j] = start_inode;
				BLOCK_BITMAP[start_inode] = i+1;
				INODE_BITMAP[start_inode] = j;
				NEXT_AVAILABLE[start_inode] = 1;
				start_inode++;
				for(k=0; k < POINTERS_PER_INODE; k++){
					if(block.inode[j].direct[k] != 0){
						NEXT_AVAILABLE[block.inode[j].direct[k]] = 1;
					}
				}
				if(block.inode[j].indirect != 0 ){
					NEXT_AVAILABLE[block.inode[j].indirect] = 1;
					disk_read(block.inode[j].indirect, block.data);
					for(p = 0; p < POINTERS_PER_BLOCK; p++ ){
						if( block.pointers[p] != 0 ){
							NEXT_AVAILABLE[block.pointers[p]] = 1;
						}
					}
				}
    
			}
		}
	}
	MOUNTED = 1; 
    return 1;
}

int fs_create()
{
    if(!MOUNTED){
        fprintf(stderr, "File system not mounted\n"); 
        return -1; 
    }
 
    int i, x; 
    union fs_block block; 
    int free_block = -1; 
    int inumber = -1; 

    NBLOCKS = disk_size(); 
    
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
            inumber = get_NEXT_AVAILABLE(); 
            BLOCK_BITMAP[inumber] = free_block;
            INODE_BITMAP[inumber] = i;
            INUMBERS[i] = inumber;

            block.inode[i].isvalid = 1;
            block.inode[i].size = 0; 
            for(x=0; x<5; x++){
                block.inode[i].direct[x]=0; 
            }
            block.inode[i].indirect = 0;  
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
	union fs_block block;
    int i, p;  
    
    inode_load(inumber, &curr); 
    
    if (curr.isvalid ==0){
        fprintf(stderr, "Error in deleting inode: does not exist\n"); 
        return 0; 
    }

	// set inode values and release all NEXT_AVAILABLES
	for(i=0; i < POINTERS_PER_INODE; i++){
		if( curr.direct[i] != 0 ){
			release_inumber(curr.direct[i]);
        	curr.direct[i] = 0; 
		}
    }
    
	if( curr.indirect != 0 ){
		disk_read(curr.indirect, block.data);
		for(p = 0; p < POINTERS_PER_BLOCK; p++ ){
			if( block.pointers[p] != 0 ){
				release_inumber(block.pointers[p]);
				block.pointers[p] = 0;
			}
		}
		disk_write(curr.indirect, block.data);
		release_inumber(curr.indirect);
    	curr.indirect = 0; 
	}
    curr.isvalid = 0; 
    curr.size = 0; 
    inode_save(inumber, &curr); 
	release_inumber(inumber);
    
    BITMAP[CURR_BLOCK]=0; 
    INODE_BITMAP[inumber] = 0;
    BLOCK_BITMAP[inumber] = 0;

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

	if (length <= 0) {
		printf("length %d invalid\n", length);
		return 0;
	}

	if (offset < 0) {
		printf("offset %d invalid\n", offset);
		return 0;
	}

    int block_pointer = determine_block(inumber, offset); 
	int offset_bytes = offset % DISK_BLOCK_SIZE;
    int bytes_read = 0; 
    int curr_indirect_block;

    while(length > 0){
		// indirect
        if(block_pointer >= POINTERS_PER_INODE){
            disk_read(curr.indirect, block.data); 
            if( block.pointers[block_pointer - POINTERS_PER_INODE] != 0 ){
                curr_indirect_block = block.pointers[block_pointer - POINTERS_PER_INODE];
            } else{
                return bytes_read; // no more data to read
            }
	    	disk_read(curr_indirect_block, block.data);
        } else { // direct
            disk_read(curr.direct[block_pointer], block.data); 
        }

		// copy data 
        if ( DISK_BLOCK_SIZE - offset_bytes > length ){
			if( bytes_read + length + offset + offset_bytes > curr.size){
				memcpy(data, &block.data[offset_bytes], curr.size - offset - offset_bytes);
            	bytes_read = curr.size - offset - offset_bytes; 
			} else{
            	memcpy(data, &block.data[offset_bytes], length);
            	bytes_read += length; 
			}
			length = 0;
		} else{
			if( bytes_read + DISK_BLOCK_SIZE + offset + offset_bytes > curr.size){
				memcpy(data, &block.data[offset_bytes], curr.size - offset - offset_bytes);
            	bytes_read = curr.size - offset - offset_bytes; 
				length = 0;
			}else{
				memcpy(data, &block.data[offset_bytes], DISK_BLOCK_SIZE);
            	bytes_read += DISK_BLOCK_SIZE; 
				length -= DISK_BLOCK_SIZE;
			}
    	}
		
    	block_pointer++; 
    }
    return bytes_read; 
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	if(!MOUNTED){
        fprintf(stderr, "File system not mounted\n"); 
        return 0; 
    }
    
	struct fs_inode curr; 
    union fs_block block; 
    int x; 
    inode_load(inumber, &curr); 
    if(curr.isvalid == 0){
        fprintf(stderr, "Inode does not exist\n"); 
        return 0; 
    }
    
	int block_pointer = determine_block(inumber, offset); 
    if(block_pointer == -1){
        fprintf(stderr, "Offset too large\n"); 
        return 0; 
    } 

	// user error checking
	if (length <= 0) {
		printf("length %d invalid\n", length);
		return 0;
	}

	if (offset < 0) {
		printf("offset %d invalid\n", offset);
		return 0;
	}

    int offset_bytes = offset % DISK_BLOCK_SIZE;
    int bytes_written = 0;
    int curr_indirect_block; 
    while(length > 0){ //b block_pointer < POINTERS_PER_BLOCK){

        if(block_pointer == POINTERS_PER_INODE){ // creates space for indirect
            curr.indirect = get_NEXT_AVAILABLE();
			if(curr.indirect == -1){
				return 0; // out of space
			}
            disk_read(curr.indirect, block.data); 
            for(x=0; x<POINTERS_PER_BLOCK; x++){
                block.pointers[x] = 0; 
            }
            disk_write(curr.indirect, block.data); 
        }
		// gets block.data with disk read
        if(block_pointer >= POINTERS_PER_INODE){ // indirect
            disk_read(curr.indirect, block.data); 
            for(x=0; x<POINTERS_PER_BLOCK; x++){
                if(block.pointers[x] == 0){
                    block.pointers[x] = get_NEXT_AVAILABLE();
					if( block.pointers[x] == -1 ){
						return 0; // out of space
					}
                    disk_write(curr.indirect, block.data);
                    curr_indirect_block = block.pointers[x]; 
                    break; 
                }
            }
            disk_read(curr_indirect_block, block.data); 
        }else { // direct
            curr.direct[block_pointer] = get_NEXT_AVAILABLE(); 
			if( curr.direct[block_pointer] == -1 ){
				return 0;
			}
            disk_read(curr.direct[block_pointer], block.data); 
        }
		// copy data
        if ( DISK_BLOCK_SIZE - offset_bytes > length){
			memcpy(&block.data[offset_bytes], data + bytes_written, length);
			bytes_written += length;
			length = 0;
		} else{
			memcpy(&block.data[offset_bytes], data + bytes_written, DISK_BLOCK_SIZE - offset_bytes);
			bytes_written += DISK_BLOCK_SIZE - offset_bytes;
			length -=  DISK_BLOCK_SIZE - offset_bytes;
		}
		// write data to disk
        if(block_pointer >= 5){
            disk_write(curr_indirect_block, block.data); 
        }else {
            disk_write(curr.direct[block_pointer], block.data); 
        }

        //update inode size
        block_pointer++;
    }   
    curr.size = bytes_written + offset;
    inode_save(inumber, &curr);
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
    block_num = offset / 4096; 
    return block_num; 
}

int get_NEXT_AVAILABLE(){
	int i;
	for ( i = 1; i < NBLOCKS; i++ ){
		if( NEXT_AVAILABLE[i] == 0 ){
			NEXT_AVAILABLE[i] = 1;
			return i;
		}
	}

	printf("Error: The disk is full.\n");
	return -1; // completely full
}

void release_inumber(int inumber){
	NEXT_AVAILABLE[inumber] = 0;
}
