#ifndef USERPROG_FD_H
#define USERPROG_FD_H
#include <stdbool.h>
#include "threads/vaddr.h"
#include "filesys/file.h"

#define FDSIZE (PGSIZE / 8)

typedef struct file *fd_list[FDSIZE];

int fd_open(const char *, fd_list);
int fd_filesize(int, fd_list);
int fd_read(int, void *, unsigned, fd_list);
int fd_write(int, const void *, unsigned, fd_list);
void fd_seek(int, unsigned, fd_list);
unsigned fd_tell(int, fd_list);
void fd_close(int, fd_list);
int fd_dup2(int, int, fd_list);

void fd_close_all(fd_list);
bool fd_dup_fd_list(fd_list, fd_list);

#endif /* USERPROG_FD_H */