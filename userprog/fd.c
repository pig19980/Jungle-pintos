#include "userprog/fd.h"
#include <stdio.h>

bool fd_create(const char *file, unsigned initial_size) { return false; }
bool fd_remove(const char *file) { return false; }
int fd_open(const char *file) { return -1; }
int fd_filesize(int fd) { return -1; }
int fd_read(int fd, void *buffer, unsigned size) { return -1; }
int fd_write(int fd, const void *buffer, unsigned size) {
	printf("%s", buffer);
	return -1;
}
void fd_seek(int fd, unsigned position) { return; }
unsigned fd_tell(int fd) { return 0; }
void fd_close(int fd) { return; }