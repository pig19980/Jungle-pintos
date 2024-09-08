#include "userprog/fd.h"
#include <stddef.h>
#include "filesys/filesys.h"

static bool check_fd(int fd) {
	if (0 <= fd && fd < FDSIZE)
		return true;
	else
		return false;
}

int fd_open(const char *file, fd_list fd_list) {
	struct file *ret_file;

	ret_file = filesys_open(file);
	if (ret_file) {
		for (int fd = 0; fd < FDSIZE; ++fd) {
			if (!fd_list[fd]) {
				fd_list[fd] = ret_file;
				return fd;
			}
		}
		file_close(ret_file);
	}

	return -1;
}

int fd_filesize(int fd, fd_list fd_list) {
	struct file *file;
	int ret;

	if (!check_fd(fd)) {
		return 0;
	}

	file = fd_list[fd];
	if (!file || file == stdin || file == stdout) {
		return 0;
	}
	ret = file_length(file);
	return ret;
}

int fd_read(int fd, void *buffer, unsigned size, fd_list fd_list) {
	struct file *file;
	int ret;

	if (!check_fd(fd)) {
		return 0;
	}
	file = fd_list[fd];
	if (file == NULL || file == stdout) {
		return 0;
	} else if (file == stdin) {
		for (unsigned got = 0; got < size; ++got) {
			input_getc(buffer + got);
		}
		return size;
	} else {
		ret = file_read(file, buffer, size);
		return ret;
	}
}

int fd_write(int fd, const void *buffer, unsigned size, fd_list fd_list) {
	struct file *file;
	int ret;

	if (!check_fd(fd)) {
		return 0;
	}
	file = fd_list[fd];
	if (file == NULL || file == stdin) {
		return 0;
	} else if (file == stdout) {
		putbuf(buffer, size);
		return size;
	} else {
		ret = file_write(file, buffer, size);
		return ret;
	}
}

void fd_seek(int fd, unsigned position, fd_list fd_list) {
	struct file *file;
	int ret;

	if (!check_fd(fd)) {
		return 0;
	}
	file = fd_list[fd];
	if (!file || file == stdin || file == stdout) {
		return;
	}
	file_seek(file, position);
}

unsigned fd_tell(int fd, fd_list fd_list) {
	struct file *file;
	int ret;

	if (!check_fd(fd)) {
		return 0;
	}
	file = fd_list[fd];
	if (!file || file == stdin || file == stdout) {
		return 0;
	}
	return file_tell(file);
}

void fd_close(int fd, fd_list fd_list) {
	struct file *file;
	int ret;

	if (!check_fd(fd)) {
		return 0;
	}
	file = fd_list[fd];
	if (file == stdin || file == stdout) {
		fd_list[fd] = NULL;
	} else if (file) {
		file_close(file);
		fd_list[fd] = NULL;
	}
}

int fd_dup2(int oldfd, int newfd, fd_list fd_list) {
	struct file *oldfile, *newfile;

	if (!check_fd(oldfd) || !check_fd(newfd)) {
		return -1;
	}
	oldfile = fd_list[oldfd];
	if (oldfile == NULL) {
		return -1;
	} else if (oldfd == newfd) {
		return newfd;
	}

	newfile = fd_list[newfd];
	if (oldfile == stdin || oldfile == stdout) {
		fd_list[newfd] = oldfile;
	} else {
		fd_list[newfd] = file_plus_open_cnt(oldfile);
		if (!fd_list[newfd]) {
			fd_list[newfd] = newfile;
			return -1;
		}
	}

	if (newfile != stdin && newfile != stdout) {
		file_close(newfile);
	}
	return newfd;
}

/* Close all fd. Called in process_init and process_exit */
void fd_close_all(fd_list fd_list) {
	for (int fd = 0; fd < FDSIZE; ++fd) {
		fd_close(fd, fd_list);
	}
}