#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpointer-arith"
#pragma ide diagnostic ignored "hicpp-signed-bitwise"

#include "minifat.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

int fd;

/*
void print_disk_info_1(info_entry_t info) {
	printf("Total blocks: %d\n", info.total_block);
	printf("Available blocks: %d\n", info.available_blocks);
	printf("Block size: %d\n", info.block_size);
	printf("Blocks per sector: %d\n", info.block_per_sector);
	printf("Sector per fat: %d\n", info.sector_per_fat);
	printf("Dir entry number: %d\n", info.dir_entry_number);
}
*/

void tm_to_date(struct tm *tm, date_t *date) {
	date->day = tm->tm_mday;
	date->month = tm->tm_mon;
	date->year = 1900 + tm->tm_year;
	date->hour = tm->tm_hour;
	date->minutes = tm->tm_min;
	date->seconds = tm->tm_sec;
}

void write_block(uint32_t block, void *buffer) {
	//int fd = open(virtual_disk, O_RDWR);

	lseek(fd, block * BLOCK_SIZE, SEEK_SET);
	write(fd, buffer, BLOCK_SIZE);

	//close(fd);
}

void read_block(uint32_t block, void *buffer) {
	//int fd = open(virtual_disk, O_RDWR);

	lseek(fd, block * BLOCK_SIZE, SEEK_SET);
	read(fd, buffer, BLOCK_SIZE);

	//close(fd);
}

void write_sector(uint32_t sector, void *buffer) {
	uint32_t first_block = sector * (SECTOR_SIZE / BLOCK_SIZE);

	for (int i = 0; i < SECTOR_SIZE / BLOCK_SIZE; i++) {
		write_block(first_block + i, buffer + (i * BLOCK_SIZE));
	}
}

void read_sector(uint32_t sector, void *buffer) {
	uint32_t first_block = sector * (SECTOR_SIZE / BLOCK_SIZE);

	for (int i = 0; i < SECTOR_SIZE / BLOCK_SIZE; i++) {
		read_block(first_block + i, buffer + (i * BLOCK_SIZE));
	}
}

void write_fat_table(void *fat, int size) {
	char buffer[SECTOR_SIZE];

	for (int i = 0; i < size; i++) {
		memcpy(buffer, fat + (i * SECTOR_SIZE), SECTOR_SIZE);
		write_sector(i + 1, buffer);
	}
}

int format(int size) {
	int blocks_count = size;// /BLOCK_SIZE;
	int sector_count = blocks_count / (SECTOR_SIZE / BLOCK_SIZE);
	char buffer[SECTOR_SIZE];

	//fat_entry_t* fat_entry = (fat_entry_t*) malloc(sizeof(fat_entry_t) * sector_count);
	//memset(fat_entry, UNUSED, sizeof(fat_entry_t) * sector_count);

	//void* table = fat_entry;

	int qtd_sectors_for_fat_table = (int) ceil(((double) sizeof(fat_entry_t) * sector_count) / SECTOR_SIZE);

	// write fat entry
	memset(buffer, UNUSED, SECTOR_SIZE);
	for (int i = 0; i < qtd_sectors_for_fat_table; i++) {
		write_sector(i + 1, buffer);
	}

	// free(fat_entry);

	// write fat infos
	int blocks_per_sector = SECTOR_SIZE / BLOCK_SIZE;
	info_entry_t info = {.total_block = blocks_count,
			.available_blocks = blocks_count - (blocks_per_sector) - (blocks_per_sector * qtd_sectors_for_fat_table) -
								(blocks_per_sector),
			.block_size = BLOCK_SIZE,
			.block_per_sector = blocks_per_sector,
			.sector_per_fat = qtd_sectors_for_fat_table,
			.dir_entry_number = DIRENTRYCOUNT};

	memset(buffer, 0x0, SECTOR_SIZE);
	memcpy(buffer, &info, sizeof(info_entry_t));
	write_sector(0, buffer);

	// write root dir entry
	memset(buffer, 0x0, SECTOR_SIZE);
	write_sector(qtd_sectors_for_fat_table + 1, buffer);

	return 0;
}

void init(info_entry_t *info, fat_entry_t **fat_entry, dir_entry_t **root_dir) {
	char buffer[SECTOR_SIZE];

	// read fat info
	read_block(0, buffer);
	memcpy(info, buffer, sizeof(info_entry_t));

	// malloc and read fat entry
	*fat_entry = (fat_entry_t *) malloc(sizeof(fat_entry_t) * (info->available_blocks / info->block_per_sector));
	for (uint32_t i = 0; i < info->sector_per_fat; i++) {
		read_sector(i + 1, buffer);
		memcpy((void *) *fat_entry + (i * SECTOR_SIZE), buffer, SECTOR_SIZE);
	}

	// malloc and read root dir entry
	*root_dir = (dir_entry_t *) malloc(SECTOR_SIZE);
	read_sector((int) info->sector_per_fat + 1, *root_dir);
}

/*
void release(fat_entry_t **fat_entry, dir_entry_t **root_dir) {
	free(*fat_entry);
	free(*root_dir);

	*fat_entry = NULL;
	*root_dir = NULL;
}
*/

int get_first_empty_dir_entry(const dir_entry_t *dir_entry, int size) {
	for (int i = 0; i < size; i++) {
		if (dir_entry[i].mode == EMPTY_TYPE) return i;
	}

	return -1;
}

int get_first_empty_fat_entry(const fat_entry_t *fat, int size) {
	for (int i = 0; i < size; i++) {
		if (fat[i] == UNUSED) return i;
	}

	return -1;
}

int
create_empty_file(dir_entry_t *dir, dir_entry_t *dir_entry_list, info_entry_t *info, fat_entry_t *fat, const char *name,
				  mode_t mode, uid_t uid, gid_t gid) {
	time_t now;
	struct tm *time_info;
	dir_entry_t new_file;

	int index_in_dir_entry = get_first_empty_dir_entry(dir_entry_list, info->dir_entry_number);
	if (index_in_dir_entry == -1) return -1;
	int index_in_fat_entry = get_first_empty_fat_entry(fat, info->available_blocks);
	if (index_in_fat_entry == -1) return -1;

	time(&now);
	time_info = localtime(&now);

	strcpy(new_file.name, name);
	new_file.mode = mode;
	new_file.uid = uid;
	new_file.gid = gid;
	new_file.size = 0;
	tm_to_date(time_info, &new_file.create);
	tm_to_date(time_info, &new_file.update);
	new_file.first_block = index_in_fat_entry + 1;

	memcpy(&dir_entry_list[index_in_dir_entry], &new_file, sizeof(dir_entry_t));// = new_file;
	fat[index_in_fat_entry] = ENDOFCHAIN;

	// jump fat sectors, info sector and enter this block
	uint32_t sector_to_write;
	if (dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + dir->first_block;
	write_sector(sector_to_write, dir_entry_list);
	write_fat_table(fat, info->sector_per_fat);

	return 0;
}

int
create_empty_dir(dir_entry_t *dir, dir_entry_t *dir_entry_list, info_entry_t *info, fat_entry_t *fat, const char *name,
				 mode_t mode, uid_t uid, gid_t gid) {
	time_t now;
	struct tm *time_info;
	dir_entry_t new_file;

	int index_in_dir_entry = get_first_empty_dir_entry(dir_entry_list, info->dir_entry_number);
	if (index_in_dir_entry == -1) return -1;
	int index_in_fat_entry = get_first_empty_fat_entry(fat, info->available_blocks);
	if (index_in_fat_entry == -1) return -1;

	time(&now);
	time_info = localtime(&now);

	strcpy(new_file.name, name);
	new_file.mode = mode;
	new_file.uid = uid;
	new_file.gid = gid;
	new_file.size = 0;
	tm_to_date(time_info, &new_file.create);
	tm_to_date(time_info, &new_file.update);
	new_file.first_block = index_in_fat_entry + 1;

	memcpy(&dir_entry_list[index_in_dir_entry], &new_file, sizeof(dir_entry_t));// = new_file;
	fat[index_in_fat_entry] = ENDOFCHAIN;

	// jump fat sectors, info sector and enter this block
	if (dir == NULL)
		write_sector((int) info->sector_per_fat + 1, dir_entry_list);
	else
		write_sector((int) info->sector_per_fat + 1 + dir->first_block, dir_entry_list);

	char buffer[SECTOR_SIZE];
	memset(buffer, 0x0, SECTOR_SIZE);
	write_sector(info->sector_per_fat + 1 + new_file.first_block, buffer);

	write_fat_table(fat, info->sector_per_fat);

	return 0;
}

int search_dir_entry(dir_entry_t *dir_entry, info_entry_t *info, const char *name, dir_descriptor_t *descriptor) {
	char buffer[SECTOR_SIZE];

	for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
		if (dir_entry[i].mode != EMPTY_TYPE) {
			if (strcmp(dir_entry[i].name, name) == 0 && S_ISDIR(dir_entry[i].mode)) {
				memcpy(&descriptor->dir_infos, &dir_entry[i], sizeof(dir_entry_t));

				memset(buffer, 0, SECTOR_SIZE);
				read_sector((uint32_t) info->sector_per_fat + 1 + dir_entry[i].first_block, buffer);
				memcpy(descriptor->entry, buffer, SECTOR_SIZE);

				return 1;
			}
		}
	}

	return -1;
}

int search_file_in_dir(dir_entry_t *dir_entry, const char *name, dir_entry_t *file) {
	for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
		if (dir_entry[i].mode != EMPTY_TYPE) {
			if (strcmp(dir_entry[i].name, name) == 0 && S_ISREG(dir_entry[i].mode)) {
				memcpy(file, &dir_entry[i], sizeof(dir_entry_t));

				return 1;
			}
		}
	}

	return -1;
}

int search_file_index_in_dir(dir_entry_t *dir_entry, const char *name) {
	for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
		if (dir_entry[i].mode != EMPTY_TYPE) {
			if (strcmp(dir_entry[i].name, name) == 0 && S_ISREG(dir_entry[i].mode)) {
				return (int) i;
			}
		}
	}

	return -1;
}

int search_dir_index_in_dir(dir_entry_t *dir_entry, const char *name) {
	for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
		if (dir_entry[i].mode != EMPTY_TYPE) {
			if (strcmp(dir_entry[i].name, name) == 0 && S_ISDIR(dir_entry[i].mode)) {
				return (int) i;
			}
		}
	}

	return -1;
}

int get_empty_fat_entry_number(const fat_entry_t *fat, int size) {
	int counter = 0;
	for (int i = 0; i < size; i++) {
		if (fat[i] == UNUSED) counter++;
	}

	return counter;
}

int
write_file(fat_entry_t *fat, const info_entry_t *info, dir_entry_t *dir, dir_entry_t *dir_entry_list, dir_entry_t *file,
		   int offset, const char *buffer, int size) {
	char sector_buffer[SECTOR_SIZE];

	if (file == NULL) return -1;
	if (offset > file->size) return -1;
	if (offset < 0) return -1;

	// get the number of empty bytes in last sector and a number of used sectors
	int used_bytes_last_block = file->size;
	int number_of_blocks = 1;
	while (used_bytes_last_block > SECTOR_SIZE) {
		used_bytes_last_block -= SECTOR_SIZE;
		number_of_blocks++;
	}
//    int empty_bytes_in_last_block = SECTOR_SIZE - used_bytes_last_block;
	int number_of_blocks_required = (int) ceil((double) (file->size + size) / SECTOR_SIZE);

	// get sector of offset
	int remain_offset = offset;
	while (remain_offset > SECTOR_SIZE - 1) {
		remain_offset -= SECTOR_SIZE;
	}

	int sector_to_begin_write = file->first_block;
	int offset_sub = offset;
	while (offset_sub > SECTOR_SIZE + 1) {
		sector_to_begin_write = fat[sector_to_begin_write - 1];
		offset_sub -= SECTOR_SIZE;
	}

	// data to write fit in one sector
	if ((remain_offset + size) <= SECTOR_SIZE && number_of_blocks_required <= number_of_blocks) {
		read_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);
		memcpy(&sector_buffer[remain_offset], buffer, size);
		write_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);
	} else {
		int blocks_number_to_write = (int) ceil((double) size / SECTOR_SIZE);
		int number_available_sectors = get_empty_fat_entry_number(fat, info->sector_per_fat);
		if (blocks_number_to_write > number_available_sectors) return -1;

		int qtd_write = size;
		// escreve os dados no restante do bloco
		if (remain_offset > 0 || offset == 0) {
			int remain_buffer_size = SECTOR_SIZE - remain_offset;

			read_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);
			memcpy(&sector_buffer[remain_offset], buffer, remain_buffer_size);
			write_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);

			qtd_write -= remain_buffer_size;
		}

		while (qtd_write > 0) {

			int sector_status = fat[sector_to_begin_write - 1];
			if (sector_status == ENDOFCHAIN) {
				int new_fat_input = get_first_empty_fat_entry(fat, info->available_blocks) + 1;

				fat[sector_to_begin_write - 1] = new_fat_input;
				fat[new_fat_input - 1] = ENDOFCHAIN;

				sector_to_begin_write = new_fat_input;
				write_fat_table(fat, info->sector_per_fat);
			} else {
				sector_to_begin_write = sector_status;
			}

			read_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);

			if (qtd_write > SECTOR_SIZE) {
				memcpy(sector_buffer, buffer, SECTOR_SIZE);
				qtd_write = qtd_write - SECTOR_SIZE;
			} else {
				memcpy(sector_buffer, &buffer[size - qtd_write], qtd_write);
				qtd_write = 0;
			}
			write_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);

		}
	}

	// update file in dir entry
	time_t now;
	struct tm *time_info;
	time(&now);
	time_info = localtime(&now);

	// update file size and update time
	if (offset + size > file->size)
		file->size = offset + size;
	tm_to_date(time_info, &file->update);
	int file_index = search_file_index_in_dir(dir_entry_list, file->name);
	memcpy(&dir_entry_list[file_index], file, sizeof(dir_entry_t));

	// update dir entry list
	uint32_t sector_to_write;
	if (dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + dir->first_block;
	write_sector(sector_to_write, dir_entry_list);

	return size;
}

int read_file(const fat_entry_t *fat, const info_entry_t *info, dir_entry_t *file, int offset, char *buffer,
			  unsigned int size) {
	if (fat == NULL) return -1;
	if (info == NULL) return -1;
	if (file == NULL) return -1;
	if (buffer == NULL) return -1;
	if (offset > file->size) return -1;

	char sector_buffer[SECTOR_SIZE];

	// get sector of offset
	int first_sector_offset = offset;
	while (first_sector_offset > SECTOR_SIZE - 1) {
		first_sector_offset -= SECTOR_SIZE;
	}

	int sector_to_read = file->first_block;
	int offset_sub = offset;
	while (offset_sub > SECTOR_SIZE + 1) {
		sector_to_read = fat[sector_to_read - 1];
		offset_sub -= SECTOR_SIZE;
	}

	// a file offset, left elements from buffer and buffer offset
	unsigned int file_offset = offset;
	unsigned int buffer_left = size;
	unsigned int buffer_offset = 0;

	do {
		// calculate how many bytes copy
		unsigned int copy_length = SECTOR_SIZE - first_sector_offset;

		// if bytes remain in buffer is greater buffer left elements
		if (copy_length > buffer_left)
			copy_length = buffer_left;
		// if copy value extends file size
		if (file_offset + copy_length > file->size)
			copy_length = file->size - file_offset;

		// copy value
		read_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_read, sector_buffer);
		memcpy(&buffer[buffer_offset], &sector_buffer[first_sector_offset], copy_length);

		buffer_left -= copy_length;
		buffer_offset += copy_length;
		file_offset += copy_length;

		// if file end
		if (file_offset >= file->size)
			return (int) (size - buffer_left);

		// if need get next sector
		if (first_sector_offset + copy_length >= SECTOR_SIZE) {
			sector_to_read = fat[sector_to_read - 1];
			// if end of file return, else, new sector loaded
			if (sector_to_read == ENDOFCHAIN) return (int) (size - buffer_left);
			else first_sector_offset = 0;
		}

	} while (buffer_left > 0);

	return (int) size;
}

int delete_file(fat_entry_t *fat, const info_entry_t *info, dir_entry_t *dir, dir_entry_t *dir_entry_list,
				dir_entry_t *file) {
	int file_index = search_file_index_in_dir(dir_entry_list, file->name);
	if (file_index == -1) return -1;

	int sector = file->first_block;
	int stop = 0;
	while (!stop) {
		int fat_status = fat[sector - 1];
		if (fat_status == ENDOFCHAIN) {
			fat[sector - 1] = UNUSED;
			stop = 1;
		} else {
			fat[sector - 1] = UNUSED;
			sector = fat_status;
		}
	}
	file->mode = 0;

	memcpy(&dir_entry_list[file_index], file, sizeof(dir_entry_t));

	// update dir entry list
	uint32_t sector_to_write;
	if (dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + dir->first_block;
	write_sector(sector_to_write, dir_entry_list);

	return 1;
}

int delete_dir(fat_entry_t *fat, const info_entry_t *info, dir_entry_t *father_dir, dir_entry_t *dir_entry_list,
			   dir_entry_t *dir) {
	int file_index = search_dir_index_in_dir(dir_entry_list, dir->name);
	if (file_index == -1) return -1;

	int sector = dir->first_block;
	fat[sector - 1] = UNUSED;
	dir->mode = 0;

	memcpy(&dir_entry_list[file_index], dir, sizeof(dir_entry_t));

	// update dir entry list
	uint32_t sector_to_write;
	if (father_dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + dir->first_block;
	write_sector(sector_to_write, dir_entry_list);

	return 1;
}

#pragma clang diagnostic pop