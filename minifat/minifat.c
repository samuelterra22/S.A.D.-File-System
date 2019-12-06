#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wpointer-arith"
#pragma ide diagnostic ignored "hicpp-signed-bitwise"

#include "minifat.h"
#include "../serial/serial.h"
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>

int fd;

/**
 * Function to convert struct tm to date_t
 * @param tm
 * @param date
 */
void tm_to_date(struct tm *tm, date_t *date) {
	date->day = tm->tm_mday;
	date->month = tm->tm_mon;
	date->year = 1900 + tm->tm_year;
	date->hour = tm->tm_hour;
	date->minutes = tm->tm_min;
	date->seconds = tm->tm_sec;
}

#if USING_ARDUINO == 0
/**
 * Write one block in sd card connected in computer
 * @param block
 * @param buffer
 */
void write_block(uint32_t block, char *buffer) {
	lseek(fd, block * BLOCK_SIZE, SEEK_SET);
	write(fd, buffer, BLOCK_SIZE);
}
#endif

/**
 * Function to read one block in Arduino
 * @param block
 * @param buffer
 */
void read_block(uint32_t block, void *buffer) {

#if USING_ARDUINO == 1
    char buffer_read_command[COMMAND_SIZE];

    // cria o quadro para enviar o comando de ler um bloco para o arduino
    buffer_read_command[COMMAND_POS] = READ_BLOCK;
    uint32_t* block_to_read = (uint32_t*) &buffer_read_command[BLOCK_POS];
    *block_to_read = block;

    memset(buffer, 0, BLOCK_SIZE); // limpa o buffer
    serialport_write(fd, buffer_read_command, COMMAND_SIZE); // send command to read block block
    serialport_read_until(fd, buffer, BLOCK_SIZE, UART_READ_TIMEOUT); // read block and save in buffer
#else
	lseek(fd, block * BLOCK_SIZE, SEEK_SET);
	read(fd, buffer, BLOCK_SIZE);
#endif
}

/**
 * Function to write sector in sd card
 * @param sector
 * @param buffer
 */
void write_sector(uint32_t sector, void *buffer) {
    // calculate first block from sector
	uint32_t first_block = sector * (SECTOR_SIZE / BLOCK_SIZE);

#if USING_ARDUINO == 0
    for (int i = 0; i < SECTOR_SIZE / BLOCK_SIZE; i++) {
		write_block(first_block + i, buffer + (i * BLOCK_SIZE));
	}
#else
    char buffer_write_command[COMMAND_SIZE];

    // send command to write sector
    buffer_write_command[COMMAND_POS] = WRITE_BLOCK;
    uint32_t* block_to_write = (uint32_t*) &buffer_write_command[BLOCK_POS];
    *block_to_write = first_block;

    // send command and sector to write
    serialport_write(fd, buffer_write_command, COMMAND_SIZE);
    serialport_write(fd, buffer, SECTOR_SIZE);
#endif
}

/**
 * Function to read one sector
 * @param sector
 * @param buffer
 */
void read_sector(uint32_t sector, void *buffer) {
    // calculate first block to read
	uint32_t first_block = sector * (SECTOR_SIZE / BLOCK_SIZE);

	// loop to read all blocks from sector
	for (int i = 0; i < SECTOR_SIZE / BLOCK_SIZE; i++) {
		read_block(first_block + i, buffer + (i * BLOCK_SIZE));
	}
}

/**
 * Function to write FAT table in sd card
 * @param fat
 * @param size
 */
void write_fat_table(void *fat, int size) {
	char buffer[SECTOR_SIZE];

	for (int i = 0; i < size; i++) {
        // copia para o buffer parte da tabela fat
		memcpy(buffer, fat + (i * SECTOR_SIZE), SECTOR_SIZE);
		write_sector(i + 1, buffer); // escreve o buffer no arduino
	}
}

/**
 * Function to format card
 * @param size - card size in blocks
 * @return
 */
int format(int size) {
	int blocks_count = size; // pega a quantidade de blocos que existe
	int sector_count = blocks_count / (SECTOR_SIZE / BLOCK_SIZE); // calcula a quantidade de setores existe no cartao

	char buffer[SECTOR_SIZE];

	// calcula a quantidade de blocos necessarios para escrever a tabela de fat
	int qtd_sectors_for_fat_table = (int) ceil(((double) sizeof(fat_entry_t) * sector_count) / SECTOR_SIZE);

	// escreve na tabela fat que todos os blocos estao desocupados
	memset(buffer, UNUSED, SECTOR_SIZE);
	for (int i = 0; i < qtd_sectors_for_fat_table; i++) {
		write_sector(i + 1, buffer);
	}

	// insert data to info struct
	int blocks_per_sector = SECTOR_SIZE / BLOCK_SIZE;
	info_entry_t info = {.total_block = blocks_count,
			.available_blocks = blocks_count - (blocks_per_sector) - (blocks_per_sector * qtd_sectors_for_fat_table) -
								(blocks_per_sector),
			.block_size = BLOCK_SIZE,
			.block_per_sector = blocks_per_sector,
			.sector_per_fat = qtd_sectors_for_fat_table,
			.dir_entry_number = DIRENTRYCOUNT};

	// write info struct in sector 0
	memset(buffer, 0x0, SECTOR_SIZE);
	memcpy(buffer, &info, sizeof(info_entry_t));
	write_sector(0, buffer);

	// write root dir entry
	memset(buffer, 0x0, SECTOR_SIZE);
	write_sector(qtd_sectors_for_fat_table + 1, buffer);

	return 0;
}

/**
 *
 * @param info
 * @param fat_entry
 * @param root_dir
 */
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

/**
 *
 * @param dir_entry
 * @param size
 * @return
 */
int get_first_empty_dir_entry(const dir_entry_t *dir_entry, int size) {
    // busca a proxima entrada livre no vetor de entradas do diretorio
	for (int i = 0; i < size; i++) {
		if (dir_entry[i].mode == EMPTY_TYPE) return i;
	}

	return -1;
}

/**
 *
 * @param fat
 * @param size
 * @return
 */
int get_first_empty_fat_entry(const fat_entry_t *fat, int size) {
    // pega a proxima entrada livre na tabela fat
	for (int i = 0; i < size; i++) {
		if (fat[i] == UNUSED) return i;
	}

	return -1;
}

/**
 * Cria um arquivo vazio
 * @param dir - entrada do diretorio pai (NULL caso diretorio root)
 * @param dir_entry_list - Lista de entradas do diretório pai
 * @param info - informacoes do fs
 * @param fat - tabela fat
 * @param name - nome do arquivo
 * @param mode - modo do arquivo
 * @param uid - uid do dono
 * @param gid - uid do grupo
 * @return - 0 se criado, -1 se ocorreu erro
 */
int
create_empty_file(dir_entry_t *dir, dir_entry_t *dir_entry_list, info_entry_t *info, fat_entry_t *fat, const char *name,
				  mode_t mode, uid_t uid, gid_t gid) {
	time_t now;
	struct tm *time_info;
	dir_entry_t new_file;

	// pega uma nova entrada no diretorio e na fat
	int index_in_dir_entry = get_first_empty_dir_entry(dir_entry_list, info->dir_entry_number);
	if (index_in_dir_entry == -1) return -1;
	int index_in_fat_entry = get_first_empty_fat_entry(fat, info->available_blocks);
	if (index_in_fat_entry == -1) return -1;

	// pega o tempo atual
	time(&now);
	time_info = localtime(&now);

	// insere as informacoes na entrada do novo arquivo
	strcpy(new_file.name, name);
	new_file.mode = mode;
	new_file.uid = uid;
	new_file.gid = gid;
	new_file.size = 0;
	tm_to_date(time_info, &new_file.create);
	tm_to_date(time_info, &new_file.update);
	new_file.first_block = index_in_fat_entry + 1;

	// copia a nova entrada para a lista de entradas do diretorio
	memcpy(&dir_entry_list[index_in_dir_entry], &new_file, sizeof(dir_entry_t));
	// reserva o bloco na tabela fat
	fat[index_in_fat_entry] = ENDOFCHAIN;

	// jump fat sectors, info sector and enter this block
	uint32_t sector_to_write;
	if (dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + dir->first_block;
	write_sector(sector_to_write, dir_entry_list);

	// escreve a tabela fat
	write_fat_table(fat, info->sector_per_fat);

	return 0;
}

/**
 * Cria um diretorio vazio
 * @param dir - entrada do diretorio pai (NULL caso diretorio root)
 * @param dir_entry_list - Lista de entradas do diretório pai
 * @param info - informacoes do fs
 * @param fat - tabela fat
 * @param name - nome do diretorio
 * @param mode - modo do diretorio
 * @param uid - uid do dono
 * @param gid - uid do grupo
 * @return - 0 se criado, -1 se ocorreu erro
 */
int
create_empty_dir(dir_entry_t *dir, dir_entry_t *dir_entry_list, info_entry_t *info, fat_entry_t *fat, const char *name,
				 mode_t mode, uid_t uid, gid_t gid) {
	time_t now;
	struct tm *time_info;
	dir_entry_t new_file;

    // pega uma nova entrada no diretorio e na fat
	int index_in_dir_entry = get_first_empty_dir_entry(dir_entry_list, info->dir_entry_number);
	if (index_in_dir_entry == -1) return -1;
	int index_in_fat_entry = get_first_empty_fat_entry(fat, info->available_blocks);
	if (index_in_fat_entry == -1) return -1;

    // pega o tempo atual
	time(&now);
	time_info = localtime(&now);

    // insere as informacoes na entrada do novo arquivo
	strcpy(new_file.name, name);
	new_file.mode = mode;
	new_file.uid = uid;
	new_file.gid = gid;
	new_file.size = 0;
	tm_to_date(time_info, &new_file.create);
	tm_to_date(time_info, &new_file.update);
	new_file.first_block = index_in_fat_entry + 1;

    // copia a nova entrada para a lista de entradas do diretorio
	memcpy(&dir_entry_list[index_in_dir_entry], &new_file, sizeof(dir_entry_t));
    // reserva o bloco na tabela fat
	fat[index_in_fat_entry] = ENDOFCHAIN;

	// jump fat sectors, info sector and enter this block
	if (dir == NULL)
		write_sector((int) info->sector_per_fat + 1, dir_entry_list);
	else
		write_sector((int) info->sector_per_fat + 1 + dir->first_block, dir_entry_list);

	// escreve um setor limpo no bloco, tornando assim o diretorio vazio (remove possivel lixo)
	char buffer[SECTOR_SIZE];
	memset(buffer, 0x0, SECTOR_SIZE);
	write_sector(info->sector_per_fat + 1 + new_file.first_block, buffer);

	// escreve a tabela fat
	write_fat_table(fat, info->sector_per_fat);

	return 0;
}

/**
 * Busca um diretorio em um vetor de entradas
 * @param dir_entry - vetor de entradas
 * @param info - informacoes do fs
 * @param name - nome do diretorio
 * @param descriptor - descritor de diretorio
 * @return 1 caso exista, -1 caso contrario
 */
int search_dir_entry(dir_entry_t *dir_entry, info_entry_t *info, const char *name, dir_descriptor_t *descriptor) {
	char buffer[SECTOR_SIZE];

	// percorre todas as entradas de um diretorio
	for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
	    // caso a entrada nao for vazia (arquivo excluido)
		if (dir_entry[i].mode != EMPTY_TYPE) {
		    // caso seja o nome e seja um diretorio
			if (strcmp(dir_entry[i].name, name) == 0 && S_ISDIR(dir_entry[i].mode)) {
			    // copia a entrada para o campo de entrada do descritor
				memcpy(&descriptor->dir_infos, &dir_entry[i], sizeof(dir_entry_t));

				// copia as entradas desse diretorio (filhos) para o campo de entradas do diretorio no descritor
				memset(buffer, 0, SECTOR_SIZE);
				read_sector((uint32_t) info->sector_per_fat + 1 + dir_entry[i].first_block, buffer);
				memcpy(descriptor->entry, buffer, SECTOR_SIZE);

				return 1;
			}
		}
	}

	// caso nao exista esse diretorio
	return -1;
}

/**
 * Busca um arquivo em um vetor de entradas
 * @param dir_entry - vetor de entradas (diretorio pai)
 * @param name - nome do arquivo
 * @param file - entrada que esta sendo buscada
 * @return 1 caso encontrada, -1 caso contrario
 */
int search_file_in_dir(dir_entry_t *dir_entry, const char *name, dir_entry_t *file) {
    // percorre o vetor de entradas (dir_entry)
	for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
	    // caso o arquivo exista
		if (dir_entry[i].mode != EMPTY_TYPE) {
		    // caso o nome seja igual e seja um arquivo
			if (strcmp(dir_entry[i].name, name) == 0 && S_ISREG(dir_entry[i].mode)) {
			    // copia a entrada encontrada
				memcpy(file, &dir_entry[i], sizeof(dir_entry_t));

				return 1;
			}
		}
	}

	// caso nao exista essa entrada
	return -1;
}

/**
 * Busca uma entrada qualquer (arquivo ou diretorio) em um vetor de entradas
 * @param dir_entry - vetor de entradas
 * @param name - nome da entrada
 * @param entry - valor de retorno com as informacoes
 * @return - 1 caso exista, -1 se nao
 */
int search_entry_in_dir(dir_entry_t *dir_entry, const char *name, dir_entry_t *entry) {
    // percorre o vetor inteiro
	for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
	    // caso a entrada exista
		if (dir_entry[i].mode != EMPTY_TYPE) {
		    // se o nome for igual
			if (strcmp(dir_entry[i].name, name) == 0) {
			    // copia a entrada para o retorno por referencia
				memcpy(entry, &dir_entry[i], sizeof(dir_entry_t));

				return 1;
			}
		}
	}

	// caso nao exista
	return -1;
}

/**
 * Busca o indice de um arquivo no vetor de entradas
 * @param dir_entry - vetor de entradas
 * @param name - nome do arquivo
 * @return - indice do arquivo ou -1 se nao exista
 */
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

/**
 * Busca o indice de um diretorio em um vetor de entradas
 * @param dir_entry - vetor de entradas
 * @param name - nome do diretorio
 * @return - indice do diretorio ou -1 se nao exista
 */
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

/**
 * Calcula a quantidade de entradas vazias na tabela fat
 * @param fat - tabela fat
 * @param size - tamanho da tabela fat
 * @return A quantidade de entradas vazias na FAT
 */
int get_empty_fat_entry_number(const fat_entry_t *fat, int size) {
	int counter = 0;
	for (int i = 0; i < size; i++) {
		if (fat[i] == UNUSED) counter++;
	}

	return counter;
}

/**
 * Escreve em um arquivo
 * @param fat - tabela fat
 * @param info - informacoes do fs
 * @param dir - entrada do diretorio pai (NULL se root)
 * @param dir_entry_list - vetor de entradas do diretorio pai
 * @param file - entrada do arquivo
 * @param offset - offset do arquivo
 * @param buffer - buffer com os dados a serem escritos
 * @param size - tamanho do buffer
 * @return A quantidade de bytes escritos
 */
int
write_file(fat_entry_t *fat, const info_entry_t *info, dir_entry_t *dir, dir_entry_t *dir_entry_list, dir_entry_t *file,
		   int offset, const char *buffer, int size) {
	char sector_buffer[SECTOR_SIZE];

	// verifica se tudo ok
	if (file == NULL) return -1;
	if (offset > file->size) return -1;
	if (offset < 0) return -1;

	// pega a quantidade de bytes usados no ultimo bloco e o numero de blocos usados
	int used_bytes_last_block = file->size;
	int number_of_blocks = 1;
	// enquanto a quantidade de bytes for menor que o tamanho de um setor, decrementa
	// um setor e aumenta a quantidade de blocos usados. Quando for menor, eh a quantidade
	// de bytes usados no ultimo setor
	while (used_bytes_last_block > SECTOR_SIZE) {
		used_bytes_last_block -= SECTOR_SIZE;
		number_of_blocks++;
	}

	// calcula a quantidade de blocos requeridos no pior caso (append)
	int number_of_blocks_required = (int) ceil((double) (file->size + size) / SECTOR_SIZE);

	// calcula o offset no ultimo setor
	int remain_offset = offset;
	// enquanto o offset for maior que o tamanho de um setor, vai diminuindo
	// quando for menor que um setor, eh o ponto em que o ultimo byte esta escrito naquele setor
	while (remain_offset > SECTOR_SIZE - 1) {
		remain_offset -= SECTOR_SIZE;
	}

	// busca o setor a ser escrito no arquivo
	int sector_to_begin_write = file->first_block;
	int offset_sub = offset;
	// enquanto o offset for maior que um setor, vai diminuindo, e atualizando o setor a ser
	// escrito usando a tabela fat. Quando o offset for menor, chegou no bloco desejado
	while (offset_sub > SECTOR_SIZE + 1) {
		sector_to_begin_write = fat[sector_to_begin_write - 1];
		offset_sub -= SECTOR_SIZE;
	}

	// caso o dado que quer ser escrito cabe em um setor e nao eh preciso alocar um novo setor
	if ((remain_offset + size) <= SECTOR_SIZE && number_of_blocks_required <= number_of_blocks) {
	    // le o setor que deseja escrever e envia a um buffer
		read_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);
		// escreve o buffer de entrada no arquivo
		memcpy(&sector_buffer[remain_offset], buffer, size);
		// escreve o setor no sd de novo
		write_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);
	} else {
	    // caso tenha que alocar um bloco ou nao cabe um um unico setor

	    // calcula a quantidade de blocos que deve ser escritos
		int blocks_number_to_write = (int) ceil((double) size / SECTOR_SIZE);
		// pega a quantidade de blocos disponiveis
		int number_available_sectors = get_empty_fat_entry_number(fat, info->sector_per_fat);
		// caso a quantidade nao der, retorna erro
		if (blocks_number_to_write > number_available_sectors) return -1;
		// se a tabela de fat foi alterada ou nao
		int fat_table_changed = 0;

		// cria uma variavel para manter a quantidade de bytes que deve escrever
		int qtd_write = size;
		// cria um offset de escrita do buffer
		int buffer_offset = 0;
		// escreve a quantidade de blocos restante no buffer
		if (remain_offset > 0 || offset == 0) {
		    // pega a quantidade de bytes que falta pra escrever no setor
			int remain_buffer_size = SECTOR_SIZE - remain_offset;

			// le o setor que deve escrever
			read_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);
			// copia parte do buffer para o restante do setor
			memcpy(&sector_buffer[remain_offset], &buffer[offset], remain_buffer_size);
			// escreve o setor no sd
			write_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);

			// decrementa a quantidade de bytes escritos to qua ainda falta a escrever
			qtd_write -= remain_buffer_size;
			buffer_offset += remain_buffer_size;
		}

		// enquanto a quantida que ainda tem que escrever for maior que 0
		while (qtd_write > 0) {

		    // pega o proximo bloco na fat
			int sector_status = fat[sector_to_begin_write - 1];
			// caso nao tenha nenhum bloco, ou seja, esse e ENDOFCHAIN
			if (sector_status == ENDOFCHAIN) {
			    // pega uma nova entrada na fat
				int new_fat_input = get_first_empty_fat_entry(fat, info->available_blocks) + 1;

				// atribui a entrada atual a nova entrada como proxima
				fat[sector_to_begin_write - 1] = new_fat_input;
				// faz a nova entrada ser o fim de corrente
				fat[new_fat_input - 1] = ENDOFCHAIN;

				// diz q o novo setor a ser escrito e o novo setor da fat
				sector_to_begin_write = new_fat_input;

				// sinaliza a deve modificar a tabela fat ou nao
				fat_table_changed = 1;
			} else {
			    // caso o arquivo ja tenha alocado esse espaco, pega o setor da fat
				sector_to_begin_write = sector_status;
			}

			// le o setor para o buffer
			read_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);

			// se a quantidade que deve ser escrita for maior que um setor
			if (qtd_write > SECTOR_SIZE) {
			    // preenche o setor inteiro
				memcpy(sector_buffer, &buffer[buffer_offset], SECTOR_SIZE);
				qtd_write = qtd_write - SECTOR_SIZE;
				buffer_offset += SECTOR_SIZE;
			} else {
			    // copia para o setor o restante do buffer
				memcpy(sector_buffer, &buffer[buffer_offset], qtd_write);
				qtd_write = 0;
				buffer_offset += qtd_write;
			}
			// escreve o setor modificado no sd
			write_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_begin_write, sector_buffer);
		}
        // escreve a tabela fat
		if (fat_table_changed == 1)
            write_fat_table(fat, info->sector_per_fat);
	}

	// pega o tempo atual
	time_t now;
	struct tm *time_info;
	time(&now);
	time_info = localtime(&now);

	// se o offset mais o tamanho for maior que o tamanho do arquivo, ele foi aumentado
	if (offset + size > file->size)
		file->size = offset + size;
	tm_to_date(time_info, &file->update);

	// busca o indice dessa entrada no vetor de entradas do pai
	int file_index = search_file_index_in_dir(dir_entry_list, file->name);
	// atualiza a entrada no vetor de entradas do pai
	memcpy(&dir_entry_list[file_index], file, sizeof(dir_entry_t));

	uint32_t sector_to_write;
	// caso for no diretorio root, escreve nele (logo apos a fat)
	if (dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	// caso contrario, escreve na posicao definida na entrada do diretorio pai
	else
		sector_to_write = info->sector_per_fat + 1 + dir->first_block;
	write_sector(sector_to_write, dir_entry_list);

	// retorna o tamanho que foi escrito
	return size;
}

/**
 * Le de um arquivo
 * @param fat - tabela fat
 * @param info - informacoes do fs
 * @param file - entrada do arquivo
 * @param offset - offset para leitura
 * @param buffer - buffer de destino
 * @param size - tamanho do buffer
 * @return - quantidade de bytes lidos
 */
int read_file(const fat_entry_t *fat, const info_entry_t *info, dir_entry_t *file, int offset, char *buffer,
			  unsigned int size) {
    // verifica se esta tudo certo
	if (fat == NULL) return -1;
	if (info == NULL) return -1;
	if (file == NULL) return -1;
	if (buffer == NULL) return -1;
	if (offset > file->size) return -1;

	char sector_buffer[SECTOR_SIZE];

	// busca o offset no primeiro setor a ler
	int first_sector_offset = offset;
	// enquanto nao for menor que o tamanho do setor, vai decrementando, chegando assim
	// na posicao inicial de leitura
	while (first_sector_offset > SECTOR_SIZE - 1) {
		first_sector_offset -= SECTOR_SIZE;
	}

	// busca o primeiro letor a ser lido
	int sector_to_read = file->first_block;
	int offset_sub = offset;
	// enquanto o offset for maior que um setor, atualiza o setor a ser lido com a entrada da
	// tabela FAT
	while (offset_sub > SECTOR_SIZE + 1) {
		sector_to_read = fat[sector_to_read - 1];
		offset_sub -= SECTOR_SIZE;
	}

	// o offset do arquivo, a quantidade de bytes que ainda devem ser lidos e o
	// offset do buffer de destino
	unsigned int file_offset = offset;
	unsigned int buffer_left = size;
	unsigned int buffer_offset = 0;

	do {
		// calcula quantos blocos podem ser copiados
		unsigned int copy_length = SECTOR_SIZE - first_sector_offset;

		// se a quantidade de blocos que podem ser lidos for maior que a quantidade de
		// bytes que ainda devem ser ligos, le so a quantidade de falta
		if (copy_length > buffer_left)
			copy_length = buffer_left;

		// se o valor de offset mais a quantidade que pode ser lida ultrapassa o tamanho
		// do arquivo, limita na quantidade que falta para o arquivo acabar
		if (file_offset + copy_length > file->size)
			copy_length = file->size - file_offset;

		// le o setor para um buffer
		read_sector(info->sector_per_fat + (uint32_t) 1 + sector_to_read, sector_buffer);
		// copia a quantidade para o buffer
		memcpy(&buffer[buffer_offset], &sector_buffer[first_sector_offset], copy_length);

		buffer_left -= copy_length; // decrementa a quantidade que ainda deve ser lida
		buffer_offset += copy_length; // incrementa o offset do buffer de destino
		file_offset += copy_length; // incrementa o offset no arquivo

		// se o arquivo chegou no fim, retorna a quantidade lida
		if (file_offset >= file->size)
			return (int) (size - buffer_left);

		// se deve buscar um novo setor
		if (first_sector_offset + copy_length >= SECTOR_SIZE) {
		    // pega o proximo setor que deve ser lido
			sector_to_read = fat[sector_to_read - 1];

			// se o final do arquivo foi alcancado, retornar q quantidade lida
			if (sector_to_read == ENDOFCHAIN) return (int) (size - buffer_left);
			// caso contrario, o offset do setor e 0
			else first_sector_offset = 0;
		}

	} while (buffer_left > 0);
	// faz enquanto tem coisa pra ler

	// return o tamanho lido
	return (int) size;
}

/**
 * Deleta um arquivo
 * @param fat - tabela fat
 * @param info - informacoes fo fs
 * @param dir - entrada do diretorio pai
 * @param dir_entry_list - vetor de entradas do diretorio pai
 * @param file - entrada do arquivo
 * @return
 */
int delete_file(fat_entry_t *fat, const info_entry_t *info, dir_entry_t *dir, dir_entry_t *dir_entry_list,
				dir_entry_t *file) {
    // busca o indice do arquivo no diretorio pai
	int file_index = search_file_index_in_dir(dir_entry_list, file->name);
	if (file_index == -1) return -1;

	// pega o primeiro setor usado
	int sector = file->first_block;
	int stop = 0;
	while (!stop) {
	    // busca o status desse setor
		int fat_status = fat[sector - 1];
		// se for fim de cadeia, atribui como nao usado e termina o loop
		if (fat_status == ENDOFCHAIN) {
			fat[sector - 1] = UNUSED;
			stop = 1;
		} else {
		    // caso contrario, atribui o setor como nao usado e busca o novo
			fat[sector - 1] = UNUSED;
			sector = fat_status;
		}
	}
	// modifica o arquivo para modo 0 - excluido
	file->mode = 0;

	// atualiza a tabela fat
    write_fat_table(fat, info->sector_per_fat);

    // atualiza o vetor de entradas do pai com a informacao de excluido
	memcpy(&dir_entry_list[file_index], file, sizeof(dir_entry_t));

	// busca onde deve escrever o vetor de entradas do pai
	uint32_t sector_to_write;
	if (dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + dir->first_block;
	// escreve o vetor de entradas do pai
	write_sector(sector_to_write, dir_entry_list);

	return 1;
}

/**
 * Exclui um diretorio
 * @param fat - tabela fat
 * @param info - informacoes do fs
 * @param father_dir - entrada do diretorio pai
 * @param dir_entry_list - vetor de entradas do diretorio pai
 * @param dir - entrada do diretorio
 * @return
 */
int delete_dir(fat_entry_t *fat, const info_entry_t *info, dir_entry_t *father_dir, dir_entry_t *dir_entry_list,
			   dir_entry_t *dir) {
    // busca o indice do diretorio no vetor de entradas do pai
	int file_index = search_dir_index_in_dir(dir_entry_list, dir->name);
	if (file_index == -1) return -1;

	// pega o setor do diretorio e atribui como nao nao usado
	int sector = dir->first_block;
	fat[sector - 1] = UNUSED;

	// atribui como exluido
	dir->mode = 0;

	// escreve a tabela fat
    write_fat_table(fat, info->sector_per_fat);

    // atualiza a entrada do diretorio no vetor de entradas do diretorio pai
	memcpy(&dir_entry_list[file_index], dir, sizeof(dir_entry_t));

	// calcula qual setor deve escrever o vetor de entradas do pai
	uint32_t sector_to_write;
	if (father_dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + father_dir->first_block;

	// escreve o vetor de entradas do pai
	write_sector(sector_to_write, dir_entry_list);

	return 1;
}

/**
 * Adiciona uma entrada qualquer em um vetor de entradas
 * @param dir - entrada do diretorio pai
 * @param dir_entry_list - vetor de entradas do pai
 * @param entry - entrada a adicionar
 * @param info - informacoes do fs
 * @return
 */
int
add_entry_in_dir_entry(dir_entry_t *dir, dir_entry_t *dir_entry_list, dir_entry_t *entry,
					   const info_entry_t *info) {
    // busca a primeira entrada vazia
	int index_in_dir_entry = get_first_empty_dir_entry(dir_entry_list,
													   info->dir_entry_number);
	if (index_in_dir_entry == -1) return -1;

	// copia a entrada para o vetor de entradas
	memcpy(&dir_entry_list[index_in_dir_entry], entry, sizeof(dir_entry_t));

	// calcula o setor onde deve escrever o vetor de entradas
	uint32_t sector_to_write;
	if (dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + dir->first_block;
	// escreve o vetor de entradas
	write_sector(sector_to_write, dir_entry_list);

	return 0;
}

/**
 * Atualiza uma entrada
 * @param father_dir - entrada do diretorio pai
 * @param dir_entry_list - vetor de entradas do diretorio pai
 * @param entry - entrada a ser atualizada
 * @param info - informacoes do fs
 * @param name - novo nome
 * @param uid - novo uid
 * @param gid - novo gid
 * @param mode - novo modo
 * @param update - nova data de update
 * @return
 */
int update_entry(dir_entry_t *father_dir, dir_entry_t *dir_entry_list, dir_entry_t *entry, const info_entry_t *info,
				 char *name, uid_t uid, gid_t gid, mode_t mode, date_t* update) {
    // busca se a entrada e um arquivo ou um diretorio
	int file_index = search_file_index_in_dir(dir_entry_list, entry->name);
	int dir_index = search_dir_index_in_dir(dir_entry_list, entry->name);
	// caso nao for nenhum, retorna um erro
	if (file_index == -1 && dir_index == -1) return -1;

	// atualiza os valores
	strcpy(entry->name, name);
	entry->uid = uid;
	entry->gid = gid;
	entry->mode = mode;
	if (update != NULL)
	    memcpy(&entry->update, update, sizeof(date_t));

	// atualiza em determinada possicao do vetor de entradas do pai se for um arquivo ou diretorio
	if (file_index > -1)
		memcpy(&dir_entry_list[file_index], entry, sizeof(dir_entry_t));
	else
		memcpy(&dir_entry_list[dir_index], entry, sizeof(dir_entry_t));

	// calcula onde deve escrever o vetor de entradas do pai
	uint32_t sector_to_write;
	if (father_dir == NULL)
		sector_to_write = info->sector_per_fat + 1;
	else
		sector_to_write = info->sector_per_fat + 1 + father_dir->first_block;
	// escreve o vetor de entradas do pai no sd
	write_sector(sector_to_write, dir_entry_list);

	return 1;
}

/**
 * Atualiza o tamanho do arquivo
 * @param fat - tabela fat
 * @param info - informacoes do fs
 * @param dir - entrada do diretorio pai
 * @param dir_entry_list - vetor de entradas do diretorio pai
 * @param file - entrada do arquivo
 * @param new_size - novo tamanho que o arquivo deve ter
 * @return
 */
int
resize_file(fat_entry_t *fat, const info_entry_t *info, dir_entry_t *dir, dir_entry_t *dir_entry_list, dir_entry_t *file,
            int new_size) {
    // calcula quantos bytes serao alterados
    int size_count = new_size - (int) file->size;

    // se a quantidade de bytes for maior que 0, o arquivo deve ter o tamanho aumentado
    if (size_count > 0) {

        // busca o ultimo setor do arquivo e quantos bytes estao sobrando no ultimo setor
        int sector_to_begin_write = file->first_block;
        int remain_last_sector = file->size;
        while (remain_last_sector > SECTOR_SIZE + 1) {
            sector_to_begin_write = fat[sector_to_begin_write - 1];
            remain_last_sector -= SECTOR_SIZE;
        }

        // subtrai a quantidade de bytes sobrando da quantidade que deve ter
        size_count -= remain_last_sector;

        int fat_table_changed = 0;

        // enquanto a quantidade de bytes restantes for maior que 0
        while(size_count > 0) {
            // busca uma nova entrada na tabela de fat
            int new_fat_input = get_first_empty_fat_entry(fat, info->available_blocks) + 1;

            // faz o link da entrada antiga da fat com a nova
            fat[sector_to_begin_write - 1] = new_fat_input;
            // atribui a nova entrada com final do arquivo
            fat[new_fat_input - 1] = ENDOFCHAIN;

            // atribui o novo setor final
            sector_to_begin_write = new_fat_input;
            // atribui que a tabela de fat foi modificada
            fat_table_changed = 1;

            // decrementa da quantidade o tamanho do setor
            size_count -= SECTOR_SIZE;
        }

        // caso a tabela de fat foi modificada, escreve ela novamente
        if (fat_table_changed == 1)
            write_fat_table(fat, info->sector_per_fat);

        // pega o tempo atual
        time_t now;
        struct tm *time_info;
        time(&now);
        time_info = localtime(&now);

        // atualiza o novo tamanho do arquivo
        file->size = new_size;
        tm_to_date(time_info, &file->update);

        // busca o indice do arquivo no vetor de entradas do pai
        int file_index = search_file_index_in_dir(dir_entry_list, file->name);
        // atualiza o vetor de entradas do diretorio pai
        memcpy(&dir_entry_list[file_index], file, sizeof(dir_entry_t));

        // busca o setor onde escrever o vetor de entradas do pai
        uint32_t sector_to_write;
        if (dir == NULL)
            sector_to_write = info->sector_per_fat + 1;
        else
            sector_to_write = info->sector_per_fat + 1 + dir->first_block;
        // escreve o vetor de entradas do pai
        write_sector(sector_to_write, dir_entry_list);

        return 1;
    } else if (size_count < 0) {
        // caso for negativo, deve decrementar esse tamanho
        // pega o valor absoluto que deve ser decrementado
        size_count = abs(size_count);

        // calcula a quantidade de blocos gastos pelo arquivo
        int number_of_blocks_used = (int) ceil((double) (file->size) / SECTOR_SIZE);
        // aloca um vetor com o tamanho dos blocos usados pelo arquivo
        int* blocks_used = (int*) malloc(sizeof(int) * number_of_blocks_used);

        // pega o primeiro bloco do arquivo
        int sector_to_begin_write = file->first_block;
        // o restante do ultimo setor
        int remain_last_sector = file->size;
        // o indice do vetor de blocos onde deve buscar
        int index_to_search = 0;

        // enquanto for maior que o tamanho do setor
        while (remain_last_sector > SECTOR_SIZE + 1) {
            // salva no vetor de setores usados o setor atual
            blocks_used[index_to_search++] = sector_to_begin_write;
            // pega o proximo setor na tabela fat
            sector_to_begin_write = fat[sector_to_begin_write - 1];
            // decrementa a quantidade de bytes restantes do setor
            remain_last_sector -= SECTOR_SIZE;
        }
        // calcula a quantidade de bytes usados no ultimo bloco
        int used_in_last_block = SECTOR_SIZE - remain_last_sector;

        do {
            // se a quantidade de bytes usados no ultimo setor for maior que a quantidade de
            // bytes que deve ser diminuido, pula pra baixo, porque nao eh necessario
            // desalocar nenhum setor
            if (size_count < used_in_last_block) break;

            // armazena qual setor remover
            int sector_to_remove;

            // enquanto a quantidade de bytes for maior que o numero de setores
            while(size_count > SECTOR_SIZE) {
                // le qual setor remover do vetor de blocos usados
                sector_to_remove = blocks_used[--index_to_search];
                // atribui como nao usado na tabela fat
                fat[sector_to_remove-1] = UNUSED;
                // decrementa um setor de tamanho
                size_count -= SECTOR_SIZE;
            }
            // pega o proximo setor a ser usado e o atribui como final de cadeia
            sector_to_remove = blocks_used[index_to_search];
            fat[sector_to_remove - 1] = ENDOFCHAIN;

            // atualiza a tabela fat
            write_fat_table(fat, info->sector_per_fat);
        } while (0);

        // pega o tempo atual
        time_t now;
        struct tm *time_info;
        time(&now);
        time_info = localtime(&now);

        // atualiza o tamanho do arquivo e a hora de modificacao
        file->size = new_size;
        tm_to_date(time_info, &file->update);

        // busca a entrada do arquivo no vetor do diretorio pai
        int file_index = search_file_index_in_dir(dir_entry_list, file->name);
        // atualiza o vetor de entradas
        memcpy(&dir_entry_list[file_index], file, sizeof(dir_entry_t));

        // calcula onde escrever o vetor de entradas do pai
        uint32_t sector_to_write;
        if (dir == NULL)
            sector_to_write = info->sector_per_fat + 1;
        else
            sector_to_write = info->sector_per_fat + 1 + dir->first_block;

        // escreve o vetor de entradas do diretorio pai
        write_sector(sector_to_write, dir_entry_list);
        // desaloca o vetor de blocos usados
        free(blocks_used);

        return 1;
    }

    return 0;
}

#pragma clang diagnostic pop
