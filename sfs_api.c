#include "sfs_api.h"
#include "disk_emu.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// defining variables 

// defining files
// max file 200
// max file size = 300000


// defining max block
// 13 for inode
// 2 for superblock and FBM
// 580 for datablock 
// max block = 6000




#define MAX_BLOCK 6000
#define BLOCK_SIZES 1024
#define DISK_NAME "tony_disk"
#define MAX_INODE 200
#define F_NAME_LENGTH 10
#define SUPERBLOCK_IND 0
#define FBM_IND 1
#define DIRECTORY_ENTRY 198
#define INODE_BLOCK_START 2
#define INODE_BLOCK_END 15
#define DIRECTORY_START 15
#define DIRECTORY_END 18
#define INOD_BIT_MAP_IND 18
#define DIRECTORY_BIT_MAP 19
#define DATA_BLOCK_START 20
#define DATA_BLOCK_END 6000
#define MAX_OPEN_FILE 32


// initializing the struct for inode
// direct point to the number index of the data block where the inode list is stored
// size = determine where is this inode is used
// indirect pointers to the file 
// 

typedef struct _inode_t{
	int size;
	int direct[14];
	int indirect;
} inode_t;

typedef struct _inode_bit_map{
	uint8_t map[200];
} inode_bit_map;

// initializing the struct for superblock
typedef struct _superblock_t{
	char argc[10];
	int size;
	int number_blocks;
	int number_inodes;
	inode_t root;
	//inode_t shadow[4],
	//int lastshadow,
} superblock_t;


// struct for inode table for each data block
// inode block index starts for 0 to 12 for 13 list entries
// table to store the inode entries

typedef struct _inode_table_t{
	int inode_block_index;
	inode_t table[15];
} inode_table;


// struct inside the root directory inode table
// with a size of (1*10 + 2 + 2)*200 +2 = 2802
// need 3 data block for the root directory
// f_name of 10 bytes
// indicate file size
// inode_number = it indicates which inode is the file reference to 

typedef struct _file_meta_t{
	char f_name[F_NAME_LENGTH];
	int file_size;
	int inode_number;
} file_meta_t;


// struct to store file in the directory
//
// size = total number of files in the directory of 198 files 
// file_entry = to store the meta data and address of the file
// file_bit_map to indicate which slot of the entry table is empty
// each directory can store up to 66 files
// if full go to another directory

typedef struct _directory_t{
	file_meta_t file_entry[60];
} directory_t;


// struct for the OFD table entries 
typedef struct _ofd_entry_t {
	char name[2*F_NAME_LENGTH];
	int inode_number;
	int size;
	int read_pt;
	int write_pt;
} ofd_entry_t;

// struct for the open file descriptor table 

typedef struct _ofd_t{
	int size;
	ofd_entry_t table[MAX_OPEN_FILE];
	uint8_t table_bit_map[MAX_OPEN_FILE];
} open_fd_t;

// initialize global variable
open_fd_t* open_files;


// functions to toggle, clear, and check bit for FBM and file bit map
void toggle_bit(uint8_t* bit, uint8_t pos)
{
	*bit ^= (1<<pos);
}

uint8_t check_bit(uint8_t bit, uint8_t pos)
{
	return bit & (1<<pos);
}
int check_data_block(uint8_t* bit_map, uint8_t number)
{

	return check_bit(bit_map[number/8], number%8);
}

void toggle_data_block(uint8_t* bit_map, uint8_t number)
{
	toggle_bit(&bit_map[number/8], number%8);
}

// function get index from bit map
//  check for available block and used blocks
int get_free_data_block(uint8_t* bit_map, int size)
{
	int position = -1;
	for(int i = 0; i < size; i++)
	{
		if ( check_data_block(bit_map, i) == 0)
		{
			position = i;
			break;
		}
	}
	return position;
}
// function that get the next occupied block from a starting location
int get_occupied_block(uint8_t* bit_map, int size, int start_pos)
{
	int position = -1;
	for(int i = start_pos; i < size; i++)
	{
		if (check_data_block(bit_map, i) != 0)
		{
			position = i;
			break;
		}
	}
	return position;
}

int get_free_data()
{
	// check for free data block and reserve one node
	uint8_t bit_map[1024];
	read_blocks( 1, 1, bit_map);
		// // find free data block and mark it
	int data_block_address = get_free_data_block(bit_map, MAX_BLOCK);
	toggle_data_block(bit_map, data_block_address);
	write_blocks(1, 1, bit_map);
	return data_block_address;
}

void pin_data_block( int data_block_index)
{
	uint8_t bit_map[1024];
	read_blocks( 1, 1, bit_map);
	// free data block 
	toggle_data_block(bit_map, data_block_index);
}







// make new file 

// function to initialize inode file for the file 

// function to get free inode 
int get_free_inode_index()
{
	int index = -1;
	inode_bit_map* inode_bit_map1 = (inode_bit_map*) malloc(BLOCK_SIZES);
    read_blocks(INOD_BIT_MAP_IND, 1, inode_bit_map1);
    uint8_t* map = inode_bit_map1->map;
    free(inode_bit_map1);
    //printf("AY\n");
	for (int i = 0; i < MAX_INODE -1; i++)
	{
		if ( map[i] == 0)
		{
			index = i;
			break;
		}
	}
	return index;
}
// function to get free inode and reserve it 
// return the inode number

void set_inode_index(int index, int size)
{
	inode_bit_map* inode_bit_map1 = malloc(BLOCK_SIZES);
    read_blocks(18, 1, inode_bit_map1);
    inode_bit_map1->map[index] = 1;
    write_blocks(18, 1, inode_bit_map1);
    free(inode_bit_map1);
}

void free_inode_index(int index, int size)
{
	inode_bit_map* inode_bit_map1 = malloc(BLOCK_SIZES);
    read_blocks(18, 1, inode_bit_map1);
    inode_bit_map1->map[index] = 0;
    write_blocks(18, 1, inode_bit_map1);
    free(inode_bit_map1);
}


// function to read node and put it in the buffer
void read_inode(int index, inode_t* inode)
{
	int block = index / 13 + 2;
	int entry_ind = index % 15;
	inode_table* table = malloc(BLOCK_SIZES);
	read_blocks(block, 1, table);
	*inode = table->table[entry_ind];
	free(table);
}

// function to write node and put it in the buffer 
void write_inode(int index, inode_t* inode)
{
	// inode block
	int block = index / 13 + 2;
	int entry_ind = index % 15;
	inode_table* table_inode = malloc(BLOCK_SIZES);
	read_blocks(block, 1, table_inode);
	table_inode->table[entry_ind] = *inode;
	write_blocks(block, 1, table_inode);
	free(table_inode);

	// toggle inode index
	inode_bit_map* inode_bit_map1 = malloc(BLOCK_SIZES);
    read_blocks(18, 1, inode_bit_map1);
    inode_bit_map1->map[index] = 1;
    write_blocks(18, 1, inode_bit_map1);
    free(inode_bit_map1);
}

// take in data block address 
int reserve_free_inode()
{
	int index = -1;
	index = get_free_inode_index();
	if ( index > 0){

		// set the inode bit map
		set_inode_index(index, MAX_INODE);
		// init the dummy inode
		inode_t* dummy2 = malloc(sizeof(inode_t)+100);
		dummy2->size = 0;
		// write the final inode block and write it back to the disk
		write_inode( index, dummy2);
		free(dummy2);
	}
	return index;
}


// function for directory
// find free spot in directory and return the index
int get_free_directory_index()
{
	// read the directory bit map
	uint8_t* dir_bit_map = malloc(BLOCK_SIZES);
	read_blocks(DIRECTORY_BIT_MAP, 1 , dir_bit_map);
	int index = -1;
	for(int i = 0; i < DIRECTORY_ENTRY; i++)
	{
		if (dir_bit_map[i] == 0)
		{
			index = i;
			dir_bit_map[i] = 1;
			break;
		}
	}
	write_blocks(DIRECTORY_BIT_MAP, 1, dir_bit_map);
	free(dir_bit_map);
	// return -1 if full
	return index;
}

// function that read directory node
void free_directory_node( int index)
{
	uint8_t* dir_bit_map = malloc(BLOCK_SIZES);
	read_blocks(DIRECTORY_BIT_MAP, 1 , dir_bit_map);
	dir_bit_map[index] = 0;
	write_blocks(DIRECTORY_BIT_MAP, 1, dir_bit_map);
	free(dir_bit_map);	

}

// function that read directory node and
// return inode number
//

int get_directory_entry(char* name)
{
	// read the directory bit map
	//printf("%s\n", name);
	uint8_t* dir_bit_map = malloc(BLOCK_SIZES);
	read_blocks(DIRECTORY_BIT_MAP, 1 , dir_bit_map);
	int index = -1;
	directory_t* root1 = malloc( BLOCK_SIZES);

	read_blocks(DIRECTORY_START + 0, 1, root1);
	directory_t* root2 = malloc( BLOCK_SIZES);
	read_blocks(DIRECTORY_START + 1, 1, root2);
	directory_t* root3 = malloc( BLOCK_SIZES);
	read_blocks(DIRECTORY_START + 2, 1, root3);
	int target = -1;
	for(int i = 0; i < DIRECTORY_ENTRY; i++)
	{
		if (dir_bit_map[i] == 1)
		{
			int layer = i / 60;
			int pos = i % 60;
			if ( layer == 0)
			{
				if (strcmp(root1->file_entry[pos].f_name, name) == 0 )
				{
					target = i;
					break;
				}
			} else if ( layer == 1)
			{
				if (strcmp(root2->file_entry[pos].f_name, name) == 0 )
				{
					target = i;
					break;
				}
			} else {
				if (strcmp(root3->file_entry[pos].f_name, name) == 0 )
				{
					target = i;
					break;
				}
			}
		}
	}
	free(root1);
	free(root2);
	free(root3);
	free(dir_bit_map);
	return target;
}

// function that get the directory file inode index
int read_directory_entry_inode(int index)
{
	// get the position at the directory
	int dir_num = index / 60;
	int entry_ind = index % 60;
	// read the correspond directory block
	directory_t* root = malloc(BLOCK_SIZES);
	read_blocks(DIRECTORY_START + dir_num, 1, root);
	int target = root->file_entry[entry_ind].inode_number;
	free(root);
	return target;

}


// function that write directory node into the directory
// return directory index
//
int write_directory_entry(char* name, int inode_n, int size,int index)
{
	// get the position at the directory
	int dir_num = index / 60;
	int entry_ind = index % 60;
	// read the correspond directory block
	directory_t* root = malloc(BLOCK_SIZES);
	read_blocks(DIRECTORY_START + dir_num, 1, root);
	// init the root file node
	root->file_entry[entry_ind].file_size = size;
	root->file_entry[entry_ind].inode_number = inode_n;
	strcpy(root->file_entry[entry_ind].f_name, name);
	// write the directory_node back
	write_blocks(DIRECTORY_START+dir_num, 1, root);
	free(root);
	return index;

}

// create a spot for file in the directory
int create_file_directory(char* name, int inode_n, int size)
{
	//int index = -1;
	// check the free slot in directory
	int index = get_free_directory_index();

	if (index == -1)
	{
		return -1;
	}
	// write the directory nod
	write_directory_entry(name, inode_n, size, index);
	//printf("Anthony\n");
	return index;
}


// function to create a file in OFD table
// return index in ofd 
// return -1 if failed
//

int create_file_OFD( char* name)
{
	// check whether thhe OFD table is full
	if (open_files->size > MAX_OPEN_FILE)
	{
	  	return -1;
	}

	// get free slot in the open file table
	// return index of the free slot
	// return -1 if full
	int index = -1;
	for ( int i = 0; i < MAX_OPEN_FILE; i++)
	{
		if(open_files->table_bit_map[i] == 0)
		{
			open_files->table_bit_map[i] = 1;
			index = i;
			break;
		}
	}
	if ( index < 0)
	{
		return -1;
	}
	// create the file slot in OFD
	//printf("Ant\n");
	ofd_entry_t* open_file = malloc(sizeof(ofd_entry_t)+100);
	open_file->size = 0;
	strcpy(open_file->name, name);
	open_file->inode_number = reserve_free_inode();
	//int pointers_address = get_free_data();
	open_file->read_pt = 0;
	open_file->write_pt = 0;
	int node_n = open_file->inode_number;
	// 
	// assign the file slot into OFD
	open_files->table[index] = *open_file;
	free(open_file);
	// create file in directory
	create_file_directory(name, node_n, 0);
	return index;
}

// Search open file using 

int search_open_file_id(int index)
{
	if (open_files->table_bit_map[index] == 1)
	{
		return index;
	}else {
		return -1;
	}
}
int search_open_file(char* name)
{
	for(int i = 0; i < MAX_OPEN_FILE; i++)
	{
		if (open_files->table_bit_map[i] == 1)
		{
			if( strcmp(open_files->table[i].name, name) == 0 )
			{
				return i;
			}
		}
	}
	return -1;
}
// function the free the bit map in ofd
// return 0 
int close_open_file(int file_id)
{
	if( open_files->table_bit_map[file_id] == 0)
	{
		return -1;
	}
	open_files->table_bit_map[file_id] = 0;
	return 0;
}

// function that remove a file from the system

int remove_file(char* file)
{
	// remove file entry from oft
	int open_ind = search_open_file(file);
	if ( open_ind != -1)
	{
		close_open_file(open_ind);
	}

	int dir_index = get_directory_entry(file);
	if ( dir_index == -1)
	{
		return -1;
	}


	// remove bit map on data block
	int inode_index = read_directory_entry_inode(dir_index);
	free_directory_node(dir_index);
	if ( inode_index == -1)
	{
		return -1;
	}
	// toggle data block
	inode_t* dummy = malloc(sizeof(inode_t)+100);
	read_inode( inode_index, dummy);
	for(int i = 0; i < 14; i++)
	{
		if ( dummy->direct[i] != 0)
		{
			pin_data_block(dummy->direct[i]);
		}
	}
	free_inode_index(inode_index, MAX_INODE);
	while ( dummy->indirect != 0)
	{
		int new_node = dummy->indirect;
		read_inode( inode_index, dummy);
		for(int i = 0; i < 14; i++)
		{
			if ( dummy->direct[i] != 0)
			{
				pin_data_block(dummy->direct[i]);
			}
		}
		free_inode_index(inode_index, MAX_INODE);

	}
	free(dummy);
	return 0;

}

// return last data block
int addINodeData( int inode_index, int* data, int counter)
{

	// toggle data block
	int count = 0;
	int new_node = inode_index;
	int old_node = inode_index;
	//printf("%d\n", old_node);
	inode_t* dummy = malloc(sizeof(inode_t)+100);
	read_inode( inode_index, dummy);
	for(int i = 0; i < counter; i++)
	{
		if( count < 14)
		{
			dummy->direct[i % 14] = data[i];
			count++;

		} else {

			new_node = reserve_free_inode();
			dummy->indirect = new_node;
			write_inode(old_node, dummy);
			read_inode(new_node, dummy);
			old_node = new_node;
			dummy->direct[i % 14] = data[i];
			count = 1;
		}
	}
	//printf("%d\n", dummy->direct[counter-1]);
	write_inode(old_node, dummy);
	free(dummy);
	return old_node;

}







void mkssfs(int fresh){
	// if the fresh tag is on init on a fresh disk
	if (fresh)
	{
		init_fresh_disk(DISK_NAME, BLOCK_SIZES, MAX_BLOCK);

		/// SUPERBLOCK INIT /// BLOCK #0
		// initialize superblock 
		// init all fields in the superblock
		superblock_t superblock;
		strcpy(superblock.argc, "abc");
		superblock.size = BLOCK_SIZES;
		superblock.number_blocks = MAX_BLOCK;
		superblock.number_inodes = 0;

		// initialize jnode
		// set inode_table_address to index of the data block
		superblock.root.size = 14;
		for (int i = 0; i < 14; i++)
		{
			superblock.root.direct[i] = i +2;
		}
		superblock.root.indirect = -1;

		// writing superblock into the disk
		write_blocks(SUPERBLOCK_IND, 1, &superblock);



		// FBM INIT BLOCK #1
		// initialize Free bit map
		uint8_t bit_map[1024];
		for ( int i = 0; i < 1024; i++)
		{
			bit_map[i] = 0;
		}
		// toggle all data block from 0 to 17
		// toggle data block 0
		toggle_data_block(bit_map, SUPERBLOCK_IND);
		//toggle data block 1
		toggle_data_block(bit_map, FBM_IND);
		// toggle rest until 17
		for (int i = INODE_BLOCK_START; i < INODE_BLOCK_END; i++)
		{
			toggle_data_block(bit_map, i);
		}
		for (int i = DIRECTORY_START; i < 25; i++)
		{
			toggle_data_block(bit_map, i);
		}

		// toggle_data_block(bit_map, DIRECTORY_BIT_MAP);
		// toggle_data_block(bit_map, INOD_BIT_MAP_IND);
		//printf("%u\n", check_data_block(bit_map, 15));
		write_blocks(1,1,&bit_map);


		// INODE BLOCK INIT BLOCKS # 2 - 14
		// intializing inode block list to the data blocks
		// inode list takes 13 blocks and each block has 16 inode entry
		// create 1 block with 16 empty indoe entry 

		inode_table init_inode_table;
		for (int i = 0; i < 15; i++)
		{
			init_inode_table.table[i].size = -1;
			for(int i = 0; i < 14; i++)
			{
				init_inode_table.table[i].direct[i] = 0;
			}
			init_inode_table.table[i].indirect = 0;

		}


		for (int i = 0; i < 13; i++)
		{
			init_inode_table.inode_block_index = i;
			write_blocks(INODE_BLOCK_START +i, 1, &init_inode_table);
		}

		// init inode FBM
		inode_bit_map* i_bit_map = malloc(BLOCK_SIZES);
		// first spot reserved for directory
		i_bit_map->map[0] = 1;
		for (int i = 1; i < 200; i++)
		{
			i_bit_map->map[i] = 0;
		}

		write_blocks(INOD_BIT_MAP_IND, 1, i_bit_map);
		free(i_bit_map);


		// inode_table* temp = malloc(BLOCK_SIZE);
		// read_blocks(INODE_BLOCK_END-1, 1, temp);
		// printf("%d\n", temp->inode_block_index);
		// printf("%d\n", temp->table[0].size);
		// free(temp);

		// ROOT DIRECTORY BLOCK INIT BLOCKS # 15 -17
		// with total of 3 directory data block
		// each with 66 entries of a total of 198 nodes
		// with a 9 entries 8 bit file bit map

		directory_t root_dir;
		for(int i = 0; i < 3; i++)
		{
			write_blocks(DIRECTORY_START+i, 1, &root_dir);
		}
		//setting up directory bit map
		uint8_t dir_bit_map[DIRECTORY_ENTRY];
		for(int i = 0; i < DIRECTORY_ENTRY; i++)
		{
			dir_bit_map[i] = 0;
		}
		write_blocks(DIRECTORY_BIT_MAP, 1, &dir_bit_map);


	}// else  init on a existing disk
	else {
		init_disk(DISK_NAME, BLOCK_SIZES, MAX_BLOCK);
	}



	// setting up in memory OFD table
	// 
	open_files = malloc(sizeof(open_fd_t)+1000);
	open_files->size = 0;
	for (int i = 0; i < MAX_OPEN_FILE; i++)
	{
		open_files->table_bit_map[i] = 0;
	}



}
int ssfs_fopen(char *name){
    // if the open file table is full 
    // return -1
    if ( open_files->size >= MAX_OPEN_FILE)
    {
    	return -1;
    }
    int index = search_open_file(name);
    if ( index == -1)
    {
    	index = create_file_OFD(name);
    }
    return index;
}


int ssfs_fclose(int fileID){

    return close_open_file(fileID);
}



int ssfs_frseek(int fileID, int loc){
    return 0;
}
int ssfs_fwseek(int fileID, int loc){
    return 0;
}

int ssfs_fwrite(int fileID, char *buf, int length){

	int inode_ind = read_directory_entry_inode(fileID);
	// divide the buffer into smaller chuncks
	//printf("%d \n", inode_ind);
	int data;
	int remaining = length;
	char* buffer = malloc(BLOCK_SIZES);
	int* data_arr = malloc(200);
	int counter = 0;
	while(remaining > 0)
	{
		int toCopy = (remaining > BLOCK_SIZES) ? BLOCK_SIZES : remaining;
		memcpy(buffer,buf, toCopy);
		buf += toCopy;
		remaining -= toCopy;
		data = get_free_data();
		write_blocks(data, 1, buffer);
		data_arr[counter] = data;
		counter++;
	}
	free(buffer);
	int last_write = addINodeData(inode_ind, data_arr, counter);
	// for(int i = 0 ; i< count; i++)
	// {
	// 	printf("no\n");
	// 	printf("%d\n",data_arr[i]);
	// }
	free(data_arr);
	open_files->table[fileID].size = length;
    return length;
}


// 
int getInodeData( int inode_index, int* data_arr)
{
	int count = 0;
	int new_node = inode_index;
	int old_node = inode_index;
	inode_t* dummy = malloc(sizeof(inode_t)+100);
	read_inode(inode_index, dummy);
	for(int i = 0; i < 14; i++)
	{
		if ( dummy->direct[i] != 0)
		{
			data_arr[count] = dummy->direct[i];
			count++;
		}
	}
	while ( dummy->indirect != 0)
	{
		int new_node = dummy->indirect;
		read_inode( inode_index, dummy);
		for(int i = 0; i < 14; i++)
		{
			if ( dummy->direct[i] != 0)
			{
				data_arr[count] = dummy->direct[i];
				count++;
			}
		}

	}
	free(dummy);
	return count;
}

int ssfs_fread(int fileID, char *buf, int length){

	if ( search_open_file_id(fileID) < 0)
	{
		return -1;
	}
	int inode_ind = read_directory_entry_inode(fileID);
	int remaining = length;
	char* buffer = malloc(BLOCK_SIZES);
	int data_arr[40];
	//int counter = 0;
	int data;
	int count = getInodeData(inode_ind, data_arr);
	//printf("%d\n", count);
	for( int i = 0; i < count; i++)
	{

		read_blocks(data_arr[i], 1, buffer);
		int toRead = (remaining > BLOCK_SIZES) ? BLOCK_SIZES : remaining;
		memcpy(buf, buffer, toRead);
		if ( toRead != remaining)
			buf += toRead;

		remaining -= toRead;
	}
	free(buffer);
	//printf("%s\n", buf);
	return length;
}
int ssfs_remove(char *file){
    return remove_file(file);
}

// int main()
// {
// 	mkssfs(1);
// 	test_fileopen();
// 	free(open_files);
// 	//inode_test_function();
// }
