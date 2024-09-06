#include "userprog/fd.h"
#include "filesys/file.h"
#include <stdio.h>
#include "filesys/filesys.h"
#include "userprog/process.h"

bool fd_create(const char *file, unsigned initial_size) { return false; }
bool fd_remove(const char *file) { return false; }
int fd_open(const char *file) { return -1; }
int fd_filesize(int fd) { return -1; }

int fd_read(int fd, void *buffer, unsigned size) {
	struct process *current;
	struct file *file;
	current = (struct process *)process_current();
	file = (*current->fd_list)[fd];
	if (file == NULL || file == stdout) {
		return 0;
	} else if (file == stdin) {
		for (unsigned got = 0; got < size; ++got) {
			input_getc(buffer + got);
		}
		return size;
	}
	return -1;
}

int fd_write(int fd, const void *buffer, unsigned size) {
	struct process *current;
	struct file *file;
	current = (struct process *)process_current();
	file = (*current->fd_list)[fd];
	if (file == NULL || file == stdin) {
		return 0;
	} else if (file == stdout) {
		putbuf(buffer, size);
		return size;
	}
	return -1;
}

void fd_seek(int fd, unsigned position) { return; }
unsigned fd_tell(int fd) { return 0; }
void fd_close(int fd) { return; }
int fd_dup2(int oldfd, int newfd) { return -1; }