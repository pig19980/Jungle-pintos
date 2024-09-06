#include <stdbool.h>

bool fd_create(const char *file, unsigned initial_size);
bool fd_remove(const char *file);
int fd_open(const char *file);
int fd_filesize(int fd);
int fd_read(int fd, void *buffer, unsigned size);
int fd_write(int fd, const void *buffer, unsigned size);
void fd_seek(int fd, unsigned position);
unsigned fd_tell(int fd);
void fd_close(int fd);
int fd_dup2(int oldfd, int newfd);
