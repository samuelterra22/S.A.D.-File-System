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

char dir_list[256][256];
int curr_dir_idx = -1;

char files_list[256][256];
int curr_file_idx = -1;

char files_content[256][256];
int curr_file_content_idx = -1;



info_entry_t info_sd;
fat_entry_t* fat;
dir_entry_t* root_entry;



void print_entry(dir_entry_t* entry) {
    if (entry->mode == EMPTY_TYPE) {
        fprintf(stderr, "Empty node\n");
    } else {
        fprintf(stderr, "Name: %s\n", entry->name);
        fprintf(stderr, "Mode: %o\n",  entry->mode);
        fprintf(stderr, "UID: %d\n", entry->uid);
        fprintf(stderr, "GID: %d\n", entry->gid);
        fprintf(stderr, "Size: %d\n", entry->size);
        fprintf(stderr, "Creation time: %d/%d/%d - %d:%d:%d\n", entry->create.day, entry->create.month,
               entry->create.year, entry->create.hour, entry->create.minutes, entry->create.seconds);
        fprintf(stderr, "Update time: %d/%d/%d - %d:%d:%d\n", entry->update.day, entry->update.month,
               entry->update.year, entry->update.hour, entry->update.minutes, entry->update.seconds);
        fprintf(stderr, "First Block: %d\n", entry->first_block);
    }
}

void print_dir_entry(dir_entry_t* dir) {
    for(int i = 0; i < DIRENTRYCOUNT; i++) {
        fprintf(stderr, "Node %d\n", i);
        print_entry(&dir[i]);
        fprintf(stderr, "----------\n");
    }
}



int num_of_bars(char *string) {
    int empty_spaces = 0;

    /* search empty spaces */
    for (int i = 0; i < strlen(string); ++i)
        if (string[i] == '/')
            empty_spaces++;

    return empty_spaces;
}

char** explode_path(char *string) {
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
        //ret[i] = token;
        ret[i] = malloc(sizeof(char) * strlen(token));
        strcpy(ret[i], token);
        token = strtok(NULL, s);
        i++;
    }

    return ret;
}

void delete_path(char** path, int size) {
    for(int i = 0; i < size; i++)
        free(path[i]);
    free(path);
}


void add_dir(const char *dir_name) {
	curr_dir_idx++;
	strcpy(dir_list[curr_dir_idx], dir_name);
}

int is_dir(const char *path) {
	path++; // Eliminating "/" in the path

	for (int curr_idx = 0; curr_idx <= curr_dir_idx; curr_idx++)
		if (strcmp(path, dir_list[curr_idx]) == 0)
			return 1;

	return 0;
}

void add_file(const char *filename) {
	curr_file_idx++;
	strcpy(files_list[curr_file_idx], filename);

	curr_file_content_idx++;
	strcpy(files_content[curr_file_content_idx], "");
}

int is_file(const char *path) {
	path++; // Eliminating "/" in the path

	for (int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++)
		if (strcmp(path, files_list[curr_idx]) == 0)
			return 1;

	return 0;
}

int get_file_index(const char *path) {
	path++; // Eliminating "/" in the path

	for (int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++)
		if (strcmp(path, files_list[curr_idx]) == 0)
			return curr_idx;

	return -1;
}

void write_to_file(const char *path, const char *new_content) {
	int file_idx = get_file_index(path);

	if (file_idx == -1) // No such file
		return;

	strcpy(files_content[file_idx], new_content);
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
    if (strcmp(path, "/") == 0) {
        st->st_uid = getuid();
        st->st_gid = getgid();
        st->st_atime = time(NULL);
        st->st_mtime = time(NULL);
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;

        return 0;
    }

    int number_of_bars = num_of_bars(path);
    char** bars = explode_path(path);

    dir_entry_t *actual_dir = malloc(SECTOR_SIZE);
    memcpy(actual_dir, root_entry, SECTOR_SIZE);
    dir_descriptor_t descriptor;

    for(int i = 0; i < number_of_bars-1; i++) {
        fprintf(stderr, "bars[%d] - %s\n", i, bars[i]);
        search_dir_entry(actual_dir, &info_sd, bars[i], &descriptor);
        memcpy(actual_dir, descriptor.entry, SECTOR_SIZE);
    }

    dir_descriptor_t dir_descriptor;
    dir_entry_t file;

    int dir_exist = -1;
    int file_exist = -1;

    dir_exist = search_dir_entry(actual_dir, &info_sd, bars[number_of_bars - 1], &dir_descriptor);
    file_exist = search_file_in_dir(actual_dir, bars[number_of_bars-1], &file);

	if (dir_exist == 1) {
	    st->st_uid = dir_descriptor.dir_infos.uid;
        st->st_gid = dir_descriptor.dir_infos.gid;
        st->st_atime = time(NULL);
        st->st_mtime = time(NULL);
		st->st_mode = dir_descriptor.dir_infos.mode;
		st->st_nlink = 2;
	} else if (file_exist == 1) {
        st->st_uid = file.uid;
        st->st_gid = file.gid;
        st->st_atime = time(NULL);
        st->st_mtime = time(NULL);
        st->st_mode = file.mode;
        st->st_nlink = 1;
		st->st_size = file.size;
	} else {
		return -ENOENT;
	}

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
	filler(buffer, ".", NULL, 0); // Current Directory
	filler(buffer, "..", NULL, 0); // Parent Directory

	if (strcmp("/", path) == 0) {
        for(unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
            if (root_entry[i].mode != EMPTY_TYPE) {
                filler(buffer, root_entry[i].name, NULL, 0);
            }
        }
        return 0;
	}

    int number_of_bars = num_of_bars(path);
    char** bars = explode_path(path);

    dir_entry_t* actual_dir_entry = malloc(SECTOR_SIZE);
    memcpy(actual_dir_entry, root_entry, SECTOR_SIZE);
    dir_entry_t* actual_dir = NULL;
    dir_descriptor_t descriptor;

    for(int i = 0; i < number_of_bars; i++) {
        search_dir_entry(actual_dir_entry, &info_sd, bars[i], &descriptor);
        memcpy(actual_dir_entry, descriptor.entry, SECTOR_SIZE);
        actual_dir = &descriptor.dir_infos;
    }

    for(unsigned long i = 0; i < DIRENTRYCOUNT; i++) {
        if (actual_dir_entry[i].mode != EMPTY_TYPE) {
            filler(buffer, actual_dir_entry[i].name, NULL, 0);
        }
    }

    free(actual_dir_entry);
    delete_path(bars, number_of_bars);
	return 0;
}

/******************************************************************************
 * Open a file
 *
 * Open flags are available in fi->flags. The following rules apply.
 *****************************************************************************/
static int sad_open(const char *path, struct fuse_file_info *fi) {
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
    int number_of_bars = num_of_bars(path);
    char** bars = explode_path(path);

    dir_entry_t* actual_dir_entry = malloc(SECTOR_SIZE);
    memcpy(actual_dir_entry, root_entry, SECTOR_SIZE);
    dir_entry_t* actual_dir = NULL;
    dir_descriptor_t descriptor;

    for(int i = 0; i < number_of_bars-1; i++) {
        search_dir_entry(actual_dir_entry, &info_sd, bars[i], &descriptor);
        memcpy(actual_dir_entry, descriptor.entry, SECTOR_SIZE);
        actual_dir = &descriptor.dir_infos;
    }

    dir_entry_t file;
    int file_exist = search_file_in_dir(actual_dir_entry, bars[number_of_bars-1], &file);

    int total = read_file(fat, &info_sd, &file, offset, buffer, size);

    free(actual_dir_entry);
    delete_path(bars, number_of_bars);
    return total;
}

/******************************************************************************
 * Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string. The buffer size
 * argument includes the space for the terminating null character. If the
 * linkname is too long to fit in the buffer, it should be truncated. The
 * return value should be 0 for success.
 *****************************************************************************/
int sad_readlink(const char *path, char *link, size_t size) {
	return 0;
}

/******************************************************************************
 * Create a file node
 *
 * This is called for creation of all non-directory, non-symlink nodes. If the
 * filesystem defines a create() method, then for regular files that will be
 * called instead.
 *****************************************************************************/
int sad_mknod(const char *path, mode_t mode, dev_t dev) {
    int number_of_bars = num_of_bars(path);
    char** bars = explode_path(path);

    dir_entry_t* actual_dir_entry = malloc(SECTOR_SIZE);
    memcpy(actual_dir_entry, root_entry, SECTOR_SIZE);
    dir_entry_t* actual_dir = NULL;
    dir_descriptor_t descriptor;

    for(int i = 0; i < number_of_bars-1; i++) {
        search_dir_entry(actual_dir_entry, &info_sd, bars[i], &descriptor);
        memcpy(actual_dir_entry, descriptor.entry, SECTOR_SIZE);
        actual_dir = &descriptor.dir_infos;
    }

    char name[MAXNAME];
    memset(name, 0, MAXNAME);
    strcpy(name, bars[number_of_bars-1]);

    struct fuse_context* context = fuse_get_context();
    create_empty_file(actual_dir, actual_dir_entry, &info_sd, fat, name, mode, context->uid, context->gid);

    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);

    free(actual_dir_entry);
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
    int number_of_bars = num_of_bars(path);
    char** bars = explode_path(path);

    dir_entry_t* actual_dir_entry = malloc(SECTOR_SIZE);
    memcpy(actual_dir_entry, root_entry, SECTOR_SIZE);
    dir_entry_t* actual_dir = NULL;
    dir_descriptor_t descriptor;

    for(int i = 0; i < number_of_bars-1; i++) {
        search_dir_entry(actual_dir_entry, &info_sd, bars[i], &descriptor);
        memcpy(actual_dir_entry, descriptor.entry, SECTOR_SIZE);
        actual_dir = &descriptor.dir_infos;
    }

    char name[MAXNAME];
    memset(name, 0, MAXNAME);
    strcpy(name, bars[number_of_bars-1]);

    struct fuse_context* context = fuse_get_context();
    create_empty_dir(actual_dir, actual_dir_entry, &info_sd, fat, name, mode|S_IFDIR, context->uid, context->gid);

    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);

    delete_path(bars, number_of_bars);
    free(actual_dir_entry);
	return 0;
}

/******************************************************************************
 * Remove a file
 *****************************************************************************/
int sad_unlink(const char *path) {
    int number_of_bars = num_of_bars(path);
    char** bars = explode_path(path);

    dir_entry_t* actual_dir_entry = malloc(SECTOR_SIZE);
    memcpy(actual_dir_entry, root_entry, SECTOR_SIZE);
    dir_entry_t* actual_dir = NULL;
    dir_descriptor_t descriptor;

    for(int i = 0; i < number_of_bars-1; i++) {
        search_dir_entry(actual_dir_entry, &info_sd, bars[i], &descriptor);
        memcpy(actual_dir_entry, descriptor.entry, SECTOR_SIZE);
        actual_dir = &descriptor.dir_infos;
    }

    dir_entry_t file;
    int file_exist = search_file_in_dir(actual_dir_entry, bars[number_of_bars-1], &file);

    delete_file(fat, &info_sd, actual_dir, actual_dir_entry, &file);

    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);

    delete_path(bars, number_of_bars);
    free(actual_dir_entry);

    return 0;
}

/******************************************************************************
 * Remove a directory
 *****************************************************************************/
int sad_rmdir(const char *path) {
	return 0;
}

/******************************************************************************
 * Create a symbolic link
 *****************************************************************************/
int sad_symlink(const char *path, const char *link) {
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
	return 0;
}

/******************************************************************************
 * Create a hard link to a file
 *****************************************************************************/
int sad_link(const char *path, const char *newpath) {
	return 0;
}

/******************************************************************************
 * Change the permission bits of a file
 *
 * fi will always be NULL if the file is not currenlty open, but may also be
 * NULL if the file is open.
 *****************************************************************************/
int sad_chmod(const char *path, mode_t mode) {
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
	return 0;
}

/******************************************************************************
 * Change file last access and modification times
 *****************************************************************************/
int sad_utime(const char *path, struct utimbuf *ubuf) {
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
    int number_of_bars = num_of_bars(path);
    char** bars = explode_path(path);

    dir_entry_t* actual_dir_entry = malloc(SECTOR_SIZE);
    memcpy(actual_dir_entry, root_entry, SECTOR_SIZE);
    dir_entry_t* actual_dir = NULL;
    dir_descriptor_t descriptor;

    for(int i = 0; i < number_of_bars-1; i++) {
        search_dir_entry(actual_dir_entry, &info_sd, bars[i], &descriptor);
        memcpy(actual_dir_entry, descriptor.entry, SECTOR_SIZE);
        actual_dir = &descriptor.dir_infos;
    }

    dir_entry_t file;
    int file_exist = search_file_in_dir(actual_dir_entry, bars[number_of_bars-1], &file);

    int total = write_file(fat, &info_sd, actual_dir, actual_dir_entry, &file, offset, buffer, size);

    if (actual_dir == NULL)
        memcpy(root_entry, actual_dir_entry, SECTOR_SIZE);

    delete_path(bars, number_of_bars);
    free(actual_dir_entry);
	return total;
}

/******************************************************************************
 * Get file system statistics
 *
 * The 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *****************************************************************************/
int sad_statfs(const char *path, struct statvfs *statv) {
	return 0;
}

/******************************************************************************
 * Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync(). It's not a request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor, as opposed to release
 * which is called on the close of the last file descriptor for a file. Under
 * Linux, errors returned by flush() will be passed to userspace as errors from
 * close(), so flush() is a good place to write back any cached dirty data.
 * However, many applications ignore errors on close(), and on non-Linux systems,
 * close() may succeed even if flush() returns an error. For these reasons,
 * filesystems should not assume that errors returned by flush will ever be
 * noticed or even delivered.
 *
 * NOTE: The flush() method may be called more than once for each open(). This
 * happens if more than one file descriptor refers to an open file handle, e.g.
 * due to dup(), dup2() or fork() calls. It is not possible to determine if a
 * flush is final, so each flush should be treated equally. Multiple
 * write-flush sequences are relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will be called at any particular
 * point. It may be called more times than expected, or not at all.
 ******************************************************************************/
int sad_flush(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 * Release an open file
 *
 * Release is called when there are no more references to an open file: all
 * file descriptors are closed and all memory mappings are unmapped.
 *
 * For every open() call there will be exactly one release() call with the same
 * flags and file handle. It is possible to have a file opened more than once,
 * in which case only the last release will mean, that no more reads/writes will
 * happen on the file. The return value of release is ignored.
 ******************************************************************************/
int sad_release(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 * Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data should be
 * flushed, not the meta data.
 ******************************************************************************/
int sad_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 * Open directory
 *
 * Unless the 'default_permissions' mount option is given, this method should
 * check if opendir is permitted for this directory. Optionally opendir may also
 * return an arbitrary filehandle in the fuse_file_info structure, which will be
 * passed to readdir, releasedir and fsyncdir.
 ******************************************************************************/
int sad_opendir(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 * Release directory
 *****************************************************************************/
int sad_releasedir(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 * Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data should be
 * flushed, not the meta data
 *****************************************************************************/
int sad_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
	return 0;
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
 * Check file access permissions
 *
 * This will be called for the access() system call. If the
 * 'default_permissions' mount option is given, this method is not called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *****************************************************************************/
int sad_access(const char *path, int mask) {
	return 0;
}

/******************************************************************************
 *
 *****************************************************************************/
int sad_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 *****************************************************************************/
int sad_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi) {
	return 0;
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
		.open = sad_open,
		.read = sad_read,
		.readdir = sad_readdir,
		.readlink = sad_readlink,
		.getdir = NULL, // .getdir is deprecated
		.mknod = sad_mknod,
		.mkdir = sad_mkdir,
		.unlink = sad_unlink,
		.rmdir = sad_rmdir,
		.symlink = sad_symlink,
		.rename = sad_rename,
		.link = sad_link,
		.chmod = sad_chmod,
		.chown = sad_chown,
		.truncate = sad_truncate,
		.utime = sad_utime,
		.write = sad_write,
		.statfs = sad_statfs,
		.flush = sad_flush,
		.release = sad_release,
		.fsync = sad_fsync,
		.opendir = sad_opendir,
		.releasedir = sad_releasedir,
		.fsyncdir = sad_fsyncdir,
		.init = sad_init,
		.destroy = sad_destroy,
		.access = sad_access,
		.ftruncate = sad_ftruncate,
		.fgetattr = sad_fgetattr
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

    fd = open(virtual_disk, O_RDWR);

    if (strcmp(argv[1], "-format") == 0) {
        printf("Formatting disk ..........");
        format(3862528);
        printf("   disk successfully formatted\n");

        return 0;
    }

    init(&info_sd, &fat, &root_entry);

	fprintf(stderr, "Using Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
	fuse_status = fuse_main(argc, argv, &sad_operations, NULL);

	fprintf(stderr, "fuse_main returned %d\n", fuse_status);

	return fuse_status;
}
