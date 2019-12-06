#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#pragma ide diagnostic ignored "hicpp-signed-bitwise"
#pragma clang diagnostic ignored "-Wunused-parameter"

/******************************************************************************
 * Implementação do sistema de arquivos S.A.D. utilizando biblioteca FUSE	  *
 *                                                                            *
 * File:    main.c                                                            *
 * Authors:	Arthur Alexsander Martins Teodoro                                 *
 * 			Davi Ribeiro Militani                                             *
 * 			Samuel Terra Vieira                                               *
 * Address: Universidade Federal de Lavras                                    *
 * Date:    Nov-Dez/2019                                                      *
 *****************************************************************************/
#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "minifat/minifat.h"
#include "serial/serial.h"
#include "cache/cache.h"

info_entry_t info_sd;
fat_entry_t *fat;
dir_entry_t *root_entry;

#ifdef CACHE_ENABLE
cache_entry_t cache[CACHE_MAX_SIZE];
#endif

/**
 * Conta a quantidade de barras possui um caminho
 * @param string - caminho do arquivo
 * @return - A quantidade de barras existentes no caminho
 */
int num_of_bars(const char *string) {
    int empty_spaces = 0;

    /* search empty spaces */
    for (unsigned long i = 0; i < strlen(string); ++i)
        if (string[i] == '/')
            empty_spaces++;

    return empty_spaces;
}

/**
 * Gera um vetor com cada sub-entrada de um caminho
 * @param string - caminho
 * @return vetor de string com as sub-entradas
 */
char **explode_path(const char *string) {
    const char s[2] = "/";
    char *token;
    int i = 0;
    char str[MAXPATHLENGTH];
    strncpy(str, string, MAXPATHLENGTH);

    int bars = num_of_bars(str);

    /* alloc memory for array arguments */
    char **ret = malloc(sizeof(char *) * bars);

    /* get the first token */
    token = strtok(str, "/");

    while (token != NULL) {
        ret[i] = malloc(sizeof(char) * strlen(token));
        strcpy(ret[i], token);
        token = strtok(NULL, s);
        i++;
    }

    return ret;
}

/**
 * Destroi o vetor de caminho criado
 * @param path - vetor com as sub-entradas
 * @param size - tamanho do vetor
 */
void delete_path(char **path, int size) {
    for (int i = 0; i < size; i++)
        free(path[i]);
    free(path);
}

/**
 * Busca a entrada e um vetor de entradas a partir do diretorio raiz
 * @param bars - vetor de sub-entradas
 * @param num_of_bars - numero de sub-entradas
 * @param actual_dir_entry - lista de entradas atual
 * @param actual_dir - entrada atual
 */
void find_dir_and_entries(char **bars, int num_of_bars, dir_entry_t **actual_dir_entry,
                          dir_entry_t **actual_dir) {
    // aloca o vetor de entradas atual
    *actual_dir_entry = malloc(SECTOR_SIZE);
    // copia o diretorio raiz para o vetor
    memcpy(*actual_dir_entry, root_entry, SECTOR_SIZE);

    // atribui NULL (root) para a entrada atual
    *actual_dir = NULL;
    dir_descriptor_t descriptor;
    // sinaliza que alocou memoria para o diretorio atual
    int has_malloc = 0;

#ifdef CACHE_ENABLE
    // cria o vetor para armazenar o caminho para usar na cache
    char path_to_cache[MAXPATHLENGTH];
    memset(path_to_cache, 0, MAXPATHLENGTH);

    // aloca um vetor de entradas para armazenar os valores da rodada passada
    dir_entry_t* prev_dir_entry = malloc(SECTOR_SIZE);
    // copia com o valor atual
    memcpy(prev_dir_entry, *actual_dir_entry, SECTOR_SIZE);
#endif
    // executa para todas as entradas
    for (int i = 0; i < num_of_bars; i++) {
#ifdef CACHE_ENABLE
        strcat(path_to_cache, "/");
        strcat(path_to_cache, bars[i]);

        if (has_malloc == 0) {
            *actual_dir = malloc(sizeof(dir_entry_t));
            has_malloc = 1;
        }

        int cache_return_list = cache_search_entry_list(cache, path_to_cache, &*actual_dir_entry);
        int cache_return_entry = cache_search_entry(cache, path_to_cache, *actual_dir);

        if (cache_return_list == -1 || cache_return_entry == -1) {
            search_dir_entry(prev_dir_entry, &info_sd, bars[i], &descriptor);
        }

        if (cache_return_list == -1) {
            memcpy(*actual_dir_entry, descriptor.entry, SECTOR_SIZE);
            cache_add_entry_list(cache, path_to_cache, *actual_dir_entry);
        }

        if (cache_return_entry == -1) {
            memcpy(*actual_dir, &descriptor.dir_infos, sizeof(dir_entry_t));
            cache_add_entry(cache, path_to_cache, *actual_dir);
        }

        memcpy(prev_dir_entry, *actual_dir_entry, SECTOR_SIZE);
#else
        // busca o diretorio do nome atual com base no vetor de entradas do diretorio passado
        search_dir_entry(*actual_dir_entry, &info_sd, bars[i], &descriptor);
        // copia para o vetor de entradas atual
        memcpy(*actual_dir_entry, descriptor.entry, SECTOR_SIZE);

        // caso nao alocou a entrada atual, aloca
        if (has_malloc == 0) {
            *actual_dir = malloc(sizeof(dir_entry_t));
            has_malloc = 1;
        }
        // copia para a entrada atual o valor buscado
        memcpy(*actual_dir, &descriptor.dir_infos, sizeof(dir_entry_t));
#endif
    }

#ifdef CACHE_ENABLE
    // libera a memoria do vetor passado
    free(prev_dir_entry);
#endif

}

/**
 * Busca o vetor de entradas a partir do diretorio raiz
 * @param bars - vetor de sub-entradas
 * @param num_of_bars - numero de sub-entradas
 * @param actual_dir_entry - lista de entradas atual
 */
void find_dir_entries(char **bars, int num_of_bars, dir_entry_t **actual_dir_entry) {
    // aloca o vetor de entradas
    *actual_dir_entry = malloc(SECTOR_SIZE);
    // copia o vetor de entradas root para o atual
    memcpy(*actual_dir_entry, root_entry, SECTOR_SIZE);
    dir_descriptor_t descriptor;

#ifdef CACHE_ENABLE
    char path_to_cache[MAXPATHLENGTH];
    memset(path_to_cache, 0, MAXPATHLENGTH);
#endif

    for (int i = 0; i < num_of_bars; i++) {
#ifdef CACHE_ENABLE
        strcat(path_to_cache, "/");
        strcat(path_to_cache, bars[i]);

        int cache_return = cache_search_entry_list(cache, path_to_cache, &*actual_dir_entry);
        if (cache_return == -1) {
            search_dir_entry(*actual_dir_entry, &info_sd, bars[i], &descriptor);
            memcpy(*actual_dir_entry, descriptor.entry, SECTOR_SIZE);

            cache_add_entry_list(cache, path_to_cache, *actual_dir_entry);
        }
#else
        // busca a entrada de nome bars[i] a partir do vetor de entradas atual
        search_dir_entry(*actual_dir_entry, &info_sd, bars[i], &descriptor);
        // copia o vetor de entradas obtido para o atual
        memcpy(*actual_dir_entry, descriptor.entry, SECTOR_SIZE);
#endif
    }
}

/**
 * Cria um vetor de caminho sem o destino
 * @param bars - vetor de nomes dos caminhos intermediarios
 * @param num_bars - numero de entradas
 * @param str - vetor final
 */
void get_path_without_dest(char **bars, int num_bars, char *str) {
    // limpa o caminho e concatena a barra inicial
    memset(str, 0, MAXPATHLENGTH);
    strcat(str, "/");

    for (int i = 0; i < num_bars - 1; i++) {
        // concatena o nome mais a barra
        strcat(str, bars[i]);
        if (i != num_bars-2)
            strcat(str, "/");
    }
}

/**
 * Converte o tipo date_t para o time_t
 * @param date - data
 * @return data em time_t
 */
time_t date_to_time(date_t *date) {
    time_t raw_time;
    struct tm *time_info;

    time(&raw_time);
    time_info = localtime(&raw_time);

    time_info->tm_sec = (int) date->seconds;
    time_info->tm_min = (int) date->minutes;
    time_info->tm_hour = (int) date->hour;
    time_info->tm_mday = (int) date->day;
    time_info->tm_mon = (int) date->month;
    time_info->tm_year = (int) date->year - 1900;

    return mktime(time_info);
}

/******************************************************************************
 * Get file attributes.
 *
 * Similar to stat(). The 'st_dev' and 'st_blksize' fields are ignored. The
 * 'st_ino' field is ignored except if the 'use_ino' mount option is given. In
 * that case it is passed to userspace, but libfuse and the kernel will still
 * assign a different inode for internal use (called the "nodeid").
 *
 * fi will always be NULL if the file is not currently open, but may also be
 * NULL if the file is open.
 *****************************************************************************/
static int sad_getattr(const char *path, struct stat *st) {
    // se for o diretorio root
    if (strcmp(path, "/") == 0) {
        st->st_uid = getuid();
        st->st_gid = getgid();
        st->st_atime = time(NULL);
        st->st_mtime = time(NULL);
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;

        return 0;
    }

    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas atual
    dir_entry_t *actual_dir;
    find_dir_entries(bars, number_of_bars - 1, &actual_dir);

    dir_descriptor_t dir_descriptor;
    dir_entry_t file;

    int dir_exist = -1;
    int file_exist = -1;

#ifdef CACHE_ENABLE
    int cache_dir_exist = cache_search_entry(cache, path, &dir_descriptor.dir_infos);
    int cache_file_exist = cache_search_entry(cache, path, &file);
    fprintf(stderr, "Resultados da cache buscando %s - cache_dir_exists: %d; cache_file_exist: %d\n", path, cache_dir_exist, cache_file_exist);

    if (cache_dir_exist == -1) {
        fprintf(stderr, "Foi buscar no cartao se o diretorio existe\n");
        dir_exist = search_dir_entry(actual_dir, &info_sd, bars[number_of_bars - 1],
                                     &dir_descriptor);
        if (dir_exist == 1) {
            fprintf(stderr, "Diretorio existe, vai adicionar na cache\n");
            cache_add_entry(cache, path, &dir_descriptor.dir_infos);
        }
    } else if (S_ISDIR(dir_descriptor.dir_infos.mode)){
        dir_exist = 1;
    }
    if (cache_file_exist == -1) {
        fprintf(stderr, "Foi buscar no cartao se o arquivo existe\n");
        file_exist = search_file_in_dir(actual_dir, bars[number_of_bars - 1], &file);

        if (file_exist == 1) {
            fprintf(stderr, "File existe, vai adicionar na cache\n");
            cache_add_entry(cache, path, &file);
        }
    } else if (S_ISREG(file.mode)) {
        file_exist = 1;
    }
#else
    // busca no vetor de entradas atual se existe um diretorio com o nome final do caminho
    dir_exist = search_dir_entry(actual_dir, &info_sd, bars[number_of_bars - 1],
                                 &dir_descriptor);
    // busca no vetor de entradas atual se existe um arquivo com o nome final do caminho
    file_exist = search_file_in_dir(actual_dir, bars[number_of_bars - 1], &file);
#endif

    // preenche a struct st com as informacoes lidas do sd
    if (dir_exist == 1) {
        st->st_uid = dir_descriptor.dir_infos.uid;
        st->st_gid = dir_descriptor.dir_infos.gid;
        st->st_ctime = date_to_time(&dir_descriptor.dir_infos.create);
        st->st_atime = time(NULL);
        st->st_mtime = date_to_time(&dir_descriptor.dir_infos.update);
        st->st_mode = dir_descriptor.dir_infos.mode;
        st->st_nlink = 2;
    } else if (file_exist == 1) {
        st->st_uid = file.uid;
        st->st_gid = file.gid;
        st->st_ctime = date_to_time(&file.create);
        st->st_atime = time(NULL);
        st->st_mtime = date_to_time(&file.update);
        st->st_mode = file.mode;
        st->st_nlink = 1;
        st->st_size = file.size;
    } else {
        // caso nao exista, envia um codigo de erro
        return -ENOENT;
    }

    // desaloca o vetor de entradas e os o vetor de caminhos
    free(actual_dir);
    delete_path(bars, number_of_bars);
    return 0;
}

/******************************************************************************
 * Read directory
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and passes zero
 * to the filler function's offset. The filler function will not return '1'
 * (unless an error happens), so the whole directory is read in a single
 * readdir operation.
 *
 * 2) The readdir implementation keeps track of the offsets of the directory
 * entries. It uses the offset parameter and always passes non-zero offset to
 * the filler function. When the buffer is full (or an error happens) the
 * filler function will return '1'.
 *****************************************************************************/
static int sad_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {

    // adiciona as entradas do diretorio atual e do pai
    filler(buffer, ".", NULL, 0); /* Current Directory */
    filler(buffer, "..", NULL, 0); /* Parent Directory */

    // se for o diretorio pai, busca so no root_entry ja salvo no pc
    if (strcmp("/", path) == 0) {
        for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
            if (root_entry[i].mode != EMPTY_TYPE) {
                filler(buffer, root_entry[i].name, NULL, 0);
            }
        }
        return 0;
    }

    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas atual
    dir_entry_t *actual_dir_entry;
    find_dir_entries(bars, number_of_bars, &actual_dir_entry);

    // insere no buffer as entradas do diretorio atual
    for (unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
        if (actual_dir_entry[i].mode != EMPTY_TYPE) {
            filler(buffer, actual_dir_entry[i].name, NULL, 0);
        }
    }

    // desaloca o vetor de entradas e os o vetor de caminhos
    free(actual_dir_entry);
    delete_path(bars, number_of_bars);
    return 0;
}

/******************************************************************************
 * Read data from an open file
 *
 * Read should return exactly the number of bytes requested except on EOF or
 * error, otherwise the rest of the data will be substituted with zeroes. An
 * exception to this is when the 'direct_io' mount option is specified, in
 * which case the return value of the read system call will reflect the return
 * value of this operation.
 *****************************************************************************/
static int sad_read(const char *path, char *buffer, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas atual
    dir_entry_t *actual_dir_entry;
    find_dir_entries(bars, number_of_bars - 1, &actual_dir_entry);

    // busca a entrada do arquivo no diretorio atual
    dir_entry_t file;
    search_file_in_dir(actual_dir_entry, bars[number_of_bars - 1],
                       &file);

    // chama a funcao de read do minifat e recebe o total de bytes lidos
    int total_size = read_file(fat, &info_sd, &file, offset, buffer, size);

    // desaloca o vetor de entradas e os o vetor de caminhos
    free(actual_dir_entry);
    delete_path(bars, number_of_bars);

    return total_size;
}

/******************************************************************************
 * Create a file node
 *
 * This is called for creation of all non-directory, non-symlink nodes. If the
 * filesystem defines a create() method, then for regular files that will be
 * called instead.
 *****************************************************************************/
int sad_mknod(const char *path, mode_t mode, dev_t dev) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // cria uma string e a limpa
    char name[MAXNAME];
    memset(name, 0, MAXNAME);
    // copia o nome para a string
    strcpy(name, bars[number_of_bars - 1]);

    // pega o contexto da libfuse
    struct fuse_context *context = fuse_get_context();

    // chama a funcao de criacao de um arquivo do minifat
    create_empty_file(actual_dir, actual_dir_entry, &info_sd, fat, name, mode,
                      context->uid, context->gid);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    free(actual_dir_entry);
    if (actual_dir != NULL) free(actual_dir);
    delete_path(bars, number_of_bars);

    return 0;
}

/******************************************************************************
 * Create a directory
 *
 * Note that the mode argument may not have the type specification bits set,
 * i.e. S_ISDIR(mode) can be false. To obtain the correct directory type bits
 * use mode|S_IFDIR
 *****************************************************************************/
int sad_mkdir(const char *path, mode_t mode) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // cria uma string e a limpa
    char name[MAXNAME];
    memset(name, 0, MAXNAME);
    strcpy(name, bars[number_of_bars - 1]);

    // pega o contexto da libfuse
    struct fuse_context *context = fuse_get_context();

    // chama a funcao de criacao de um diretorio do minifat
    create_empty_dir(actual_dir, actual_dir_entry, &info_sd, fat, name, mode | S_IFDIR,
                     context->uid, context->gid);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    free(actual_dir_entry);
    if (actual_dir != NULL) free(actual_dir);
    delete_path(bars, number_of_bars);

    return 0;
}

/******************************************************************************
 * Remove a file
 *****************************************************************************/
int sad_unlink(const char *path) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // busca a entrada do arquivo no diretorio atual
    dir_entry_t file;
    search_file_in_dir(actual_dir_entry, bars[number_of_bars - 1],
                       &file);

    // chama a funcao de remover arquivo do minifat
    delete_file(fat, &info_sd, actual_dir, actual_dir_entry, &file);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        // update dir entry with file mode 0 in cache
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }
    // delete file in cache
    cache_delete_entry(cache, path);
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    delete_path(bars, number_of_bars);
    if (actual_dir != NULL) free(actual_dir);
    free(actual_dir_entry);

    return 0;
}

/******************************************************************************
 * Remove a directory
 *****************************************************************************/
int sad_rmdir(const char *path) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // busca a entrada do diretorio no diretorio atual
    dir_descriptor_t dirDescriptor;
    search_dir_entry(actual_dir_entry, &info_sd, bars[number_of_bars - 1],
                     &dirDescriptor);

    // chama a funcao de remover arquivo do minifat
    delete_dir(fat, &info_sd, actual_dir, actual_dir_entry, &dirDescriptor.dir_infos);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        // update dir entry with file mode 0 in cache
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }

    // delete file in cache
    cache_delete_entry(cache, path);
    cache_delete_entry_list(cache, path);
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    delete_path(bars, number_of_bars);
    if (actual_dir != NULL) free(actual_dir);
    free(actual_dir_entry);

    return 0;
}

/******************************************************************************
 * Rename a file
 *
 * flags may be RENAME_EXCHANGE or RENAME_NOREPLACE. If RENAME_NOREPLACE is
 * specified, the filesystem must not overwrite newname if it exists and return
 * an error instead. If RENAME_EXCHANGE is specified, the filesystem must
 * atomically exchange the two files, i.e. both must exist and neither may be
 * deleted.
 *****************************************************************************/
int sad_rename(const char *path, const char *newpath) {
    // conta a quantidade de barras existe e faz o split do caminho de origem
    int number_of_bars_src = num_of_bars(path);
    char **bars_src = explode_path(path);

    // conta a quantidade de barras existe e faz o split do caminho de destino
    int number_of_bars_dest = num_of_bars(newpath);
    char **bars_dest = explode_path(newpath);

    // busca o vetor de entradas e a entrada do ultimo diretorio de origem
    dir_entry_t *actual_dir_entry_src;
    dir_entry_t *actual_dir_src;
    find_dir_and_entries(bars_src, number_of_bars_src - 1, &actual_dir_entry_src,
                         &actual_dir_src);

    // busca o vetor de entradas e a entrada do ultimo diretorio de destino
    dir_entry_t *actual_dir_entry_dest;
    dir_entry_t *actual_dir_dest;
    find_dir_and_entries(bars_dest, number_of_bars_dest - 1, &actual_dir_entry_dest,
                         &actual_dir_dest);

    // busca a entrada de origem
    dir_entry_t entry_src;
    search_entry_in_dir(actual_dir_entry_src, bars_src[number_of_bars_src - 1],
                        &entry_src);

    // faz uma copia da entrada de origem para a entrada de destino
    dir_entry_t entry_dest;
    memcpy(&entry_dest, &entry_src, sizeof(dir_entry_t));

    // troca o nome da entrada de destino
    strcpy(entry_dest.name, bars_dest[number_of_bars_dest - 1]);
    // adiciona a entrada de destino do diretorio de destino
    add_entry_in_dir_entry(actual_dir_dest, actual_dir_entry_dest, &entry_dest, &info_sd);

    char origin_path[MAXPATHLENGTH];
    char dest_path[MAXPATHLENGTH];

    // gera o caminho sem o nome final de origem e destino
    get_path_without_dest(bars_src, number_of_bars_src, origin_path);
    get_path_without_dest(bars_dest, number_of_bars_dest, dest_path);

    // compara esses caminhos, se forem igual, tem que atualizar o vetor de origem pelo vetor de entrada
    if (strcmp(origin_path, dest_path) == 0) {
        memcpy(actual_dir_entry_src, actual_dir_entry_dest, SECTOR_SIZE);
        // se ambos forem do mesmo subdiretorio, atualizar a entrada do diretorio de origem pelo de destino
        if (actual_dir_dest != NULL)
            memcpy(actual_dir_src, actual_dir_dest, sizeof(dir_entry_t));
    }

    // atualiza a entrada de origem para deletada
    update_entry(actual_dir_src, actual_dir_entry_src, &entry_src, &info_sd,
                 entry_src.name, entry_src.uid, entry_src.gid, EMPTY_TYPE, NULL);

    // se o destino for o root e a origem nao, atualiza o root com a entrada de destino
    if (strcmp(dest_path, "/") == 0 && strcmp(origin_path, dest_path) != 0)
        memcpy(root_entry, actual_dir_entry_dest, SECTOR_SIZE);
    // se o destino for o root e a origem tambem, atualiza o root com a entrada de origem
    else if (strcmp(dest_path, "/") == 0 && strcmp(origin_path, dest_path) == 0)
        memcpy(root_entry, actual_dir_entry_src, SECTOR_SIZE);
    // se a origem for root, atualiza o root com a origem
    else if (strcmp(origin_path, "/") == 0)
        memcpy(root_entry, actual_dir_entry_src, SECTOR_SIZE);

    return 0;
}

/******************************************************************************
 * Change the permission bits of a file
 *
 * fi will always be NULL if the file is not currenlty open, but may also be
 * NULL if the file is open.
 *****************************************************************************/
int sad_chmod(const char *path, mode_t mode) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // busca a entrada no diretorio atual
    dir_entry_t entry_src;
    search_entry_in_dir(actual_dir_entry, bars[number_of_bars - 1],
                        &entry_src);

    // atualiza a entrada usando a funcao do minifat
    update_entry(actual_dir, actual_dir_entry, &entry_src, &info_sd,
                 entry_src.name, entry_src.uid, entry_src.gid, mode, NULL);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        // update dir entry with file mode 0 in cache
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }
    // update file fat entry
    cache_update_entry(cache, path, &entry_src);
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    delete_path(bars, number_of_bars);
    if (actual_dir != NULL) free(actual_dir);
    free(actual_dir_entry);

    return 0;
}

/******************************************************************************
 * Change the owner and group of a file
 *
 * fi will always be NULL if the file is not currenlty open, but may also be
 * NULL if the file is open.
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is expected to reset
 * the setuid and setgid bits.
 *****************************************************************************/
int sad_chown(const char *path, uid_t uid, gid_t gid) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // busca a entrada no diretorio atual
    dir_entry_t entry;
    search_entry_in_dir(actual_dir_entry, bars[number_of_bars - 1],
                        &entry);

    // atualiza a entrada usando a funcao do minifat
    update_entry(actual_dir, actual_dir_entry, &entry, &info_sd,
                 entry.name, uid, gid, entry.mode, NULL);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        // update dir entry with file mode 0 in cache
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }
    // update file fat entry
    cache_update_entry(cache, path, &entry);
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    delete_path(bars, number_of_bars);
    if (actual_dir != NULL) free(actual_dir);
    free(actual_dir_entry);

    return 0;
}

/******************************************************************************
 * Change the size of a file
 *
 * fi will always be NULL if the file is not currenlty open, but may also be
 * NULL if the file is open.
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is expected to reset
 * the setuid and setgid bits.
 *****************************************************************************/
int sad_truncate(const char *path, off_t newsize) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // busca a entrada do arquivo no diretorio atual
    dir_entry_t file;
    search_file_in_dir(actual_dir_entry, bars[number_of_bars - 1], &file);

    // muda o tamanho do arquivo usando a funcao do minifat
    resize_file(fat, &info_sd, actual_dir, actual_dir_entry, &file, newsize);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        // update dir entry with file mode 0 in cache
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }
    // update file fat entry
    cache_update_entry(cache, path, &file);
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    delete_path(bars, number_of_bars);
    if (actual_dir != NULL) free(actual_dir);
    free(actual_dir_entry);

    return 0;
}

/******************************************************************************
 * Change file last access and modification times
 *****************************************************************************/
int sad_utime(const char *path, struct utimbuf *ubuf) {
    // cria a data de modificacao em date_t
    struct tm *timeinfo;
    timeinfo = localtime(&ubuf->modtime);
    date_t mod_time;
    tm_to_date(timeinfo, &mod_time);

    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // busca a entrada do arquivo no diretorio atual
    dir_entry_t entry;
    search_entry_in_dir(actual_dir_entry, bars[number_of_bars - 1],
                        &entry);

    // atualiza a entrada usando a funcao do minifat
    update_entry(actual_dir, actual_dir_entry, &entry, &info_sd,
                 entry.name, entry.uid, entry.gid, entry.mode, &mod_time);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        // update dir entry with file mode 0 in cache
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }
    // update file fat entry
    cache_update_entry(cache, path, &entry);
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    delete_path(bars, number_of_bars);
    if (actual_dir != NULL) free(actual_dir);
    free(actual_dir_entry);

    return 0;
}

/******************************************************************************
 * Write data to an open file
 *
 * Write should return exactly the number of bytes requested except on error. An
 * exception to this is when the 'direct_io' mount option is specified (see read operation).
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is expected to reset
 * the setuid and setgid bits.
 *****************************************************************************/
int sad_write(const char *path, const char *buffer, size_t size, off_t offset,
              struct fuse_file_info *fi) {
    // conta a quantidade de barras existe e faz o split do caminho
    int number_of_bars = num_of_bars(path);
    char **bars = explode_path(path);

    // busca o vetor de entradas e a entrada do ultimo diretorio
    dir_entry_t *actual_dir_entry;
    dir_entry_t *actual_dir;
    find_dir_and_entries(bars, number_of_bars - 1, &actual_dir_entry, &actual_dir);

    // busca a entrada do arquivo no diretorio atual
    dir_entry_t file;
    search_file_in_dir(actual_dir_entry, bars[number_of_bars - 1],
                       &file);

    // chama a funcao de escrever da minifat
    int total_size = write_file(fat, &info_sd, actual_dir, actual_dir_entry, &file, offset,
                           buffer, size);

    // caso o diretorio atual for root, atualiza a entrada dele
    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);
#ifdef CACHE_ENABLE
    else {
        // update dir entry with file mode 0 in cache
        char dir_path[MAXPATHLENGTH];
        get_path_without_dest(bars, number_of_bars, dir_path);
        int update_cache_ok = cache_update_entry_list(cache, dir_path, actual_dir_entry);
        if (update_cache_ok == -1) {
            cache_add_entry_list(cache, dir_path, actual_dir_entry);
        }
    }
    // update file fat entry
    cache_update_entry(cache, path, &file);
#endif

    // desaloca o vetor de entradas, a entrada do diretorio e os o vetor de caminhos
    delete_path(bars, number_of_bars);
    if (actual_dir != NULL) free(actual_dir);
    free(actual_dir_entry);

    return total_size;
}

/******************************************************************************
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of struct fuse_context
 * to all file operations, and as a parameter to the destroy() method. It
 * overrides the initial value provided to fuse_main() / fuse_new().
 *****************************************************************************/
void *sad_init(struct fuse_conn_info *conn) {
    fprintf(stderr, "S.A.D. Filesystem successfully initialized.");
    return 0;
}

/******************************************************************************
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *****************************************************************************/
void sad_destroy(void *userdata) {
    fprintf(stderr, "S.A.D. Filesystem successfully destroyed.");
}

/******************************************************************************
 * The file system operations:
 *
 * Most of these should work very similarly to the well known UNIX
 * file system operations.  A major exception is that instead of
 * returning an error in 'errno', the operation should return the
 * negated error value (-errno) directly.
 *
 * All methods are optional, but some are essential for a useful
 * filesystem (e.g. getattr).  Open, flush, release, fsync, opendir,
 * releasedir, fsyncdir, access, create, truncate, lock, init and
 * destroy are special purpose methods, without which a full featured
 * filesystem can still be implemented.
 *
 * In general, all methods are expected to perform any necessary
 * permission checking. However, a filesystem may delegate this task
 * to the kernel by passing the `default_permissions` mount option to
 * `fuse_new()`. In this case, methods will only be called if
 * the kernel's permission check has succeeded.
 *
 * Almost all operations take a path which can be of any length.
 *****************************************************************************/
static struct fuse_operations sad_operations = {
        .getattr = sad_getattr,
        .read = sad_read,
        .readdir = sad_readdir,
        .mknod = sad_mknod,
        .mkdir = sad_mkdir,
        .unlink = sad_unlink,
        .rmdir = sad_rmdir,
        .rename = sad_rename,
        .chmod = sad_chmod,
        .chown = sad_chown,
        .truncate = sad_truncate,
        .utime = sad_utime,
        .write = sad_write,
        .init = sad_init,
        .destroy = sad_destroy
};

/******************************************************************************
 * Main function
 *****************************************************************************/
int main(int argc, char *argv[]) {
    int fuse_status;

    if ((getuid() == 0) || (geteuid() == 0)) {
        fprintf(stderr, "Running SADFS as root opens unacceptable security holes\n");
        return EXIT_FAILURE;
    }

    //fd = open(virtual_disk, O_RDWR);
    // abre a porta serial
    fd = serialport_init("/dev/ttyUSB0", 115200);
    serialport_flush(fd);

    // caso seja para formartar o cartao
    if (strcmp(argv[1], "-format") == 0) {
        printf("Formatting disk ..........");
        // passa a quantidade de blocos a serem usados
        format(300625);
        printf("   disk successfully formatted\n");

        return EXIT_SUCCESS;
    }

    // inicia o sistema de arquivos do cartao (carrega os valores para memoria)
    init(&info_sd, &fat, &root_entry);
#ifdef CACHE_ENABLE
    init_cache(cache);
#endif

    fprintf(stderr, "Using Fuse library version %d.%d\n", FUSE_MAJOR_VERSION,
            FUSE_MINOR_VERSION);
    // inicia a minifat
    fuse_status = fuse_main(argc, argv, &sad_operations, NULL);

    fprintf(stderr, "fuse_main returned %d\n", fuse_status);

    return fuse_status;
}

#pragma clang diagnostic pop
