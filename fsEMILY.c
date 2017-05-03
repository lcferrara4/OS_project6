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
int TOTAL_INUMBERS;


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
					int curr_inode = 2;
                    printf("inode: %d\n", start_inode);   
					start_inode++;
                } else{
                    printf("inode: %d\n", INUMBERS[i * INODES_PER_BLOCK + j]); 
                } 
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
	printf("not super\n");
        return 0; 
    }

    int inode_blocks = block.super.ninodeblocks; 

    BITMAP = (int *)malloc(sizeof(int)*NBLOCKS); 
    BLOCK_BITMAP = (int *)malloc(sizeof(int)*NBLOCKS); 

    INODE_BITMAP = (int *)malloc(sizeof(int)*NBLOCKS*INODES_PER_BLOCK); 
    INUMBERS = (int *)malloc(sizeof(int)*NBLOCKS*INODES_PER_BLOCK); 

	TOTAL_INUMBERS = 1 + inode_blocks * (POINTERS_PER_INODE + INODES_PER_BLOCK * (1 + POINTERS_PER_BLOCK) );
	NEXT_AVAILABLE = (int *)malloc(sizeof(int)*TOTAL_INUMBERS);  
	for( i = 0; i < TOTAL_INUMBERS; i++ ){
		NEXT_AVAILABLE[i] = 0;
	}
 
	BITMAP[0] = 1; //superblock 
	NEXT_AVAILABLE[0] = 1;
	NEXT_AVAILABLE[1] = 1;
	int inumber;

	int start_inode = 2;
    for(i=1; i<=inode_blocks; i++){
        inuse = 0; 
        disk_read(i, block.data); 
        for(j=0; j<INODES_PER_BLOCK; j++){
            if (block.inode[j].isvalid == 1){
		printf("is valid, j = %d\n", j);
                inuse += 1;
                inumber = get_NEXT_AVAILABLE();
	            BLOCK_BITMAP[inumber] = i;
    	        INODE_BITMAP[inumber] = j;
        	//INUMBERS[i] = inumber;
		INUMBERS[(i-1) * INODES_PER_BLOCK + j] = start_inode;
		start_inode++;
            }
        }
        if (inuse == INODES_PER_BLOCK){
            BITMAP[i] = 1; 
        } else{
            BITMAP[i] = 0; 
        }
    }
   
	start_inode = 2; 
    int k, p; 
    for(i=1; i<= inode_blocks; i++){
        disk_read(i, block.data);  
        for(j=0; j<INODES_PER_BLOCK; j++){
		/*
		if (block.inode[j].isvalid == 0){
			printf("not valid\n");
		    block.inode[j].size = 0; 
		    block.inode[j].indirect = 0; 
		    for(k=0; k < POINTERS_PER_INODE; k++){
			block.inode[j].direct[k] = 0; 
		    }
			disk_write(i, block.data);
		}
		else{
		*/
		if( block.inode[j].isvalid == 1){
			printf("assigning inumber\n");
			// already in use
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
				for(p = 0; p < INODES_PER_BLOCK; p++ ){
					if( block.pointers[p] != 0 ){
						NEXT_AVAILABLE[block.pointers[p]] = 1;
						//disk_read(block.pointers[p], block.data);
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
			printf("inumber: %d\n", inumber);
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
	release_inumber(inumber);

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
	printf("offset: %d\n", offset);
    
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
	printf("Before block pointer\n");

    int block_pointer = determine_block(inumber, offset); 
    int bytes_read = 0; 
    int bytes_read_curr = 0;
    int x, curr_indirect_block;

    while(bytes_read < length){
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
        if ( bytes_read + DISK_BLOCK_SIZE > length ){
            memcpy(data + offset + bytes_read, block.data, length - bytes_read);
            bytes_read_curr = length - bytes_read;
            bytes_read = length; 
    	} else if( curr.size - (offset + bytes_read) < DISK_BLOCK_SIZE){
            memcpy(data+offset+bytes_read, block.data, curr.size - offset - bytes_read); 
            bytes_read_curr = curr.size - offset - bytes_read;
            bytes_read = length; 

		} else{
            memcpy(data + offset + bytes_read, block.data, DISK_BLOCK_SIZE);
			for( x = 0; x < DISK_BLOCK_SIZE; x++ ){
				data[x+offset+bytes_read] = block.data[x];
			}	
            bytes_read_curr = DISK_BLOCK_SIZE;
            bytes_read += bytes_read_curr; 
    	}
    	if(bytes_read >= length ){
        	printf("Stopped here\n");
        	if( curr.size < bytes_read )
            		return curr.size - offset;
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
    int x; 
    int flag = 0; 
    int INDIRECT = 0; 
    int end_of_data = 0; 
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

    int disk_offset = offset % DISK_BLOCK_SIZE;
    int bytes_written = 0;
    int leftover = 0; 
    int curr_indirect_block; 
    while(length > 0){ //b block_pointer < POINTERS_PER_BLOCK){

        if(block_pointer == POINTERS_PER_INODE){ // creates space for indirect
            curr.indirect = get_NEXT_AVAILABLE();
            disk_read(curr.indirect, block.data); 
            for(x=0; x<POINTERS_PER_BLOCK; x++){
                block.pointers[x] = 0; 
            }
            disk_write(curr.indirect, block.data); 
        }
        if(block_pointer >= POINTERS_PER_INODE){ // indirect
            disk_read(curr.indirect, block.data); 
            INDIRECT = 1; 
            for(x=0; x<POINTERS_PER_BLOCK; x++){
                if(block.pointers[x] == 0){
                    block.pointers[x] = get_NEXT_AVAILABLE();
                    disk_write(curr.indirect, block.data);
                    curr_indirect_block = block.pointers[x]; 
                    break; 
                }
            }
            disk_read(curr_indirect_block, block.data); 
        }else { // direct
            curr.direct[block_pointer] = get_NEXT_AVAILABLE(); 
            disk_read(curr.direct[block_pointer], block.data); 
        }
		// copy data
		int k;
		if(flag){
            memcpy(block.data, &data[bytes_written], leftover); 
            disk_write(curr_indirect_block, block.data);
            return bytes_written; 
        }
        
        if ( DISK_BLOCK_SIZE > length){
			memcpy(block.data, &data[offset + bytes_written], length);
			bytes_written += length;
			length = 0;
            end_of_data = offset + bytes_written + length; 
			
			/*
				for( k = 0; k < length - bytes_written -offset; k ++ ){
					block.data[k] = data[k+offset+bytes_written];
				}
				printf("data in 1: %c\n", data[bytes_written + offset+k]); 
				//printf("block data from 1: %s\n", block.data); 
				bytes_written = length;
			*/
		} else{
            int copy_length; 
            if(strlen(data) < offset+bytes_written+DISK_BLOCK_SIZE){
                copy_length = strlen(data) - offset - bytes_written; 
            }
            else{
                copy_length = DISK_BLOCK_SIZE; 
            }
            if(copy_length > 0){
			memcpy(block.data, &data[offset + bytes_written], copy_length);
			//bytes_written += DISK_BLOCK_SIZE;
            bytes_written +=copy_length;
            end_of_data = offset + bytes_written + DISK_BLOCK_SIZE; 
			length -=  copy_length;
            }
		}
		// write data to disk
        if(block_pointer >= 5){
            disk_write(curr_indirect_block, block.data); 
        }else {
            disk_write(curr.direct[block_pointer], block.data); 
        }

        printf("strlen: %d, end_of_data: %d\n", strlen(data), end_of_data); 
        //update inode size
        if(strlen(data)-offset <= end_of_data) {//&& bytes_written == length){ //end_of_data >= strlen(data)){
            printf("here %d\n", bytes_written); 
            curr.size = bytes_written + offset;
            inode_save(inumber, &curr);
            /*if(!INDIRECT){
                flag = 1; 
                leftover = bytes_written - offset; 
            } else{
                return bytes_written;
            }*/
        } 
            
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
    printf("in determine block...\n"); 
    int block_num; 
    if(offset >= 4214784){
        return  -1; 
    }

    //if(offset % 4096 == 0){
            block_num = offset / 4096; 
    //} else{
    //    block_num = offset / 4096 + 1;
    //}
    
    //if(block_num >4){
 //   block_num =5; 
    //}

    return block_num; 
}

int get_NEXT_AVAILABLE(){
    union fs_block block; 	
	disk_read(0, block.data); 
    int inode_blocks = block.super.ninodeblocks;
	
	int i;
	for ( i = 0; i < TOTAL_INUMBERS; i++ ){
		if( NEXT_AVAILABLE[i] == 0 ){
			NEXT_AVAILABLE[i] = 1;
			return i;
		}
	}
	return -1; // completely full
}

void release_inumber(int inumber){
	NEXT_AVAILABLE[inumber] = 0;
}
