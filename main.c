#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

static const char *filepath = "/file";
static const char *filename = "file";
static const char *filecontent = "I'm the content of the only file available there\n";

/******************************************************************************
 *
 ******************************************************************************/
static int sad_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	if (strcmp(path, filepath) == 0) {
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(filecontent);
		return 0;
	}

	return -ENOENT;
}

/******************************************************************************
 *
 ******************************************************************************/
static int sad_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
					   off_t offset, struct fuse_file_info *fi) {
	(void) offset;
	(void) fi;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	filler(buf, filename, NULL, 0);

	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static int sad_open(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static int sad_read(const char *path, char *buf, size_t size, off_t offset,
					struct fuse_file_info *fi) {

	if (strcmp(path, filepath) == 0) {
		size_t len = strlen(filecontent);
		if (offset >= len) {
			return 0;
		}

		if (offset + size > len) {
			memcpy(buf, filecontent + offset, len - offset);
			return len - offset;
		}

		memcpy(buf, filecontent + offset, size);
		return size;
	}

	return -ENOENT;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_readlink(const char *path, char *link, size_t size) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_mknod(const char *path, mode_t mode, dev_t dev) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_mkdir(const char *path, mode_t mode) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_unlink(const char *path) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_rmdir(const char *path) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_symlink(const char *path, const char *link) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_rename(const char *path, const char *newpath) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_link(const char *path, const char *newpath) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_chmod(const char *path, mode_t mode) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_chown(const char *path, uid_t uid, gid_t gid) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_truncate(const char *path, off_t newsize) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_utime(const char *path, struct utimbuf *ubuf) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_write(const char *path, const char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_statfs(const char *path, struct statvfs *statv) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_flush(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_release(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_opendir(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_releasedir(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
void *sad_init(struct fuse_conn_info *conn) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
void sad_destroy(void *userdata) {

}

/******************************************************************************
 *
 ******************************************************************************/
int sad_access(const char *path, int mask) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
int sad_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi) {
	return 0;
}

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
 *
 ******************************************************************************/
int main(int argc, char *argv[]) {
	int fuse_status;

	fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
	fuse_status = fuse_main(argc, argv, &sad_operations, NULL);

	fprintf(stderr, "fuse_main returned %d\n", fuse_status);

	return fuse_status;
}
