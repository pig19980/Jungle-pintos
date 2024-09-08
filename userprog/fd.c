#include "userprog/fd.h"
#include "filesys/file.h"
#include <stdio.h>
#include "filesys/filesys.h"
#include "userprog/process.h"

bool fd_create(const char *file, unsigned initial_size) {
	bool ret;
	ret = filesys_create(file, initial_size);
	return ret;
}

bool fd_remove(const char *file) {
	bool ret;
	ret = filesys_remove(file);
	return ret;
}

int fd_open(const char *file) {
	struct file *ret_file;
	struct process *current;
	ret_file = filesys_open(file);
	if (ret_file) {
		current = process_current();
		for (int fd = 0; fd < FDSIZE; ++fd) {
			if (!(*current->fd_list)[fd]) {
				(*current->fd_list)[fd] = ret_file;
				return fd;
			}
		}
		file_close(ret_file);
	}

	return -1;
}

int fd_filesize(int fd) {
	struct file *file;
	struct process *current;
	int ret;

	current = process_current();
	file = (*current->fd_list)[fd];
	if (!file || file == stdin || file == stdout) {
		return 0;
	}
	ret = file_length(file);
	return ret;
}

int fd_read(int fd, void *buffer, unsigned size) {
	struct process *current;
	struct file *file;
	int ret;

	current = (struct process *)process_current();
	file = (*current->fd_list)[fd];
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

int fd_write(int fd, const void *buffer, unsigned size) {
	struct process *current;
	struct file *file;
	int ret;

	current = (struct process *)process_current();
	file = (*current->fd_list)[fd];
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

void fd_seek(int fd, unsigned position) {
	struct process *current;
	struct file *file;
	int ret;

	current = (struct process *)process_current();
	file = (*current->fd_list)[fd];
	if (!file || file == stdin || file == stdout) {
		return;
	}
	file_seek(file, position);
}

unsigned fd_tell(int fd) {
	struct process *current;
	struct file *file;
	int ret;

	current = (struct process *)process_current();
	file = (*current->fd_list)[fd];
	if (!file || file == stdin || file == stdout) {
		return 0;
	}
	return file_tell(file);
}

void fd_close(int fd) {
	struct process *current;
	struct file *file;
	int ret;

	current = (struct process *)process_current();
	file = (*current->fd_list)[fd];
	if (file == stdin || file == stdout) {
		(*current->fd_list)[fd] = NULL;
	} else if (file) {
		file_close(file);
		(*current->fd_list)[fd] = NULL;
	}
}

int fd_dup2(int oldfd, int newfd) {
	struct process *current;
	struct file *oldfile, *newfile;

	current = (struct process *)process_current();
	oldfile = (*current->fd_list)[oldfd];
	if (oldfile == NULL) {
		return -1;
	} else if (oldfd == newfd) {
		return newfd;
	}

	newfile = (*current->fd_list)[newfd];
	(*current->fd_list)[newfd] = file_duplicate(oldfile);

	if (!(*current->fd_list)[newfd]) {
		return -1;
	}
	if (newfile != stdin && newfile != stdout) {
		file_close(newfile);
	}
	return newfd;
}