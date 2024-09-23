#include <syscall.h>
#include <stdint.h>
#include "../syscall-nr.h"

__attribute__((always_inline)) static __inline int64_t
syscall(uint64_t num_, uint64_t a1_, uint64_t a2_, uint64_t a3_, uint64_t a4_,
		uint64_t a5_, uint64_t a6_) {
	int64_t ret;
	register uint64_t *num asm("rax") = (uint64_t *)num_;
	register uint64_t *a1 asm("rdi") = (uint64_t *)a1_;
	register uint64_t *a2 asm("rsi") = (uint64_t *)a2_;
	register uint64_t *a3 asm("rdx") = (uint64_t *)a3_;
	register uint64_t *a4 asm("r10") = (uint64_t *)a4_;
	register uint64_t *a5 asm("r8") = (uint64_t *)a5_;
	register uint64_t *a6 asm("r9") = (uint64_t *)a6_;

	__asm __volatile("mov %1, %%rax\n"
					 "mov %2, %%rdi\n"
					 "mov %3, %%rsi\n"
					 "mov %4, %%rdx\n"
					 "mov %5, %%r10\n"
					 "mov %6, %%r8\n"
					 "mov %7, %%r9\n"
					 "syscall\n"
					 : "=a"(ret)
					 : "g"(num), "g"(a1), "g"(a2), "g"(a3), "g"(a4), "g"(a5),
					   "g"(a6)
					 : "cc", "memory");
	return ret;
}

/* Invokes syscall NUMBER, passing no arguments, and returns the
   return value as an `int'. */
#define syscall0(NUMBER) (syscall(((uint64_t)NUMBER), 0, 0, 0, 0, 0, 0))

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0) \
	(syscall(((uint64_t)NUMBER), ((uint64_t)ARG0), 0, 0, 0, 0, 0))
/* Invokes syscall NUMBER, passing arguments ARG0 and ARG1, and
   returns the return value as an `int'. */
#define syscall2(NUMBER, ARG0, ARG1)                                          \
	(syscall(((uint64_t)NUMBER), ((uint64_t)ARG0), ((uint64_t)ARG1), 0, 0, 0, \
			 0))

#define syscall3(NUMBER, ARG0, ARG1, ARG2)                           \
	(syscall(((uint64_t)NUMBER), ((uint64_t)ARG0), ((uint64_t)ARG1), \
			 ((uint64_t)ARG2), 0, 0, 0))

#define syscall4(NUMBER, ARG0, ARG1, ARG2, ARG3)                       \
	(syscall(((uint64_t *)NUMBER), ((uint64_t)ARG0), ((uint64_t)ARG1), \
			 ((uint64_t)ARG2), ((uint64_t)ARG3), 0, 0))

#define syscall5(NUMBER, ARG0, ARG1, ARG2, ARG3, ARG4)               \
	(syscall(((uint64_t)NUMBER), ((uint64_t)ARG0), ((uint64_t)ARG1), \
			 ((uint64_t)ARG2), ((uint64_t)ARG3), ((uint64_t)ARG4), 0))
void halt(void) {
	syscall0(SYS_HALT);
	NOT_REACHED();
}

void exit(int status) {
	syscall1(SYS_EXIT, status);
	NOT_REACHED();
}

pid_t fork(const char *thread_name) {
	return (pid_t)syscall1(SYS_FORK, thread_name);
}

int exec(const char *file) { return (pid_t)syscall1(SYS_EXEC, file); }

int wait(pid_t pid) { return syscall1(SYS_WAIT, pid); }

bool create(const char *file, unsigned initial_size) {
	return syscall2(SYS_CREATE, file, initial_size);
}

bool remove(const char *file) { return syscall1(SYS_REMOVE, file); }

int open(const char *file) { return syscall1(SYS_OPEN, file); }

int filesize(int fd) { return syscall1(SYS_FILESIZE, fd); }

int read(int fd, void *buffer, unsigned size) {
	return syscall3(SYS_READ, fd, buffer, size);
}

int write(int fd, const void *buffer, unsigned size) {
	return syscall3(SYS_WRITE, fd, buffer, size);
}

void seek(int fd, unsigned position) { syscall2(SYS_SEEK, fd, position); }

unsigned tell(int fd) { return syscall1(SYS_TELL, fd); }

void close(int fd) { syscall1(SYS_CLOSE, fd); }

int dup2(int oldfd, int newfd) { return syscall2(SYS_DUP2, oldfd, newfd); }

/*fd로 열린 파일의 오프셋(offset) 바이트부터 length 바이트 만큼을 프로세스의 가상주소공간의 주소 addr 에 매핑 합니다.
전체 파일은 addr에서 시작하는 연속 가상 페이지에 매핑됩니다. 파일 길이(length)가 PGSIZE의 배수가 아닌 경우 최종 매핑된
페이지의 일부 바이트가 파일 끝을 넘어 "stick out"됩니다. page_fault가 발생하면 이 바이트를 0으로 설정하고 페이지를 
디스크에 다시 쓸 때 버립니다. 성공하면 이 함수는 파일이 매핑된 가상 주소를 반환합니다. 실패하면 파일을 매핑하는 데 유효한
 주소가 아닌 NULL을 반환해야 합니다.*/
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset) {
	return (void *)syscall5(SYS_MMAP, addr, length, writable, fd, offset);
}

/*지정된 주소 범위 addr에 대한 매핑을 해제한다. 지정된 주소는 아직 매핑 해제되지 않은 동일한 프로세서의 mmap에
대한 이전 호출에서 반환된 가상 주소여야 한다.*/
void munmap(void *addr) { syscall1(SYS_MUNMAP, addr); }

bool chdir(const char *dir) { return syscall1(SYS_CHDIR, dir); }

bool mkdir(const char *dir) { return syscall1(SYS_MKDIR, dir); }

bool readdir(int fd, char name[READDIR_MAX_LEN + 1]) {
	return syscall2(SYS_READDIR, fd, name);
}

bool isdir(int fd) { return syscall1(SYS_ISDIR, fd); }

int inumber(int fd) { return syscall1(SYS_INUMBER, fd); }

int symlink(const char *target, const char *linkpath) {
	return syscall2(SYS_SYMLINK, target, linkpath);
}

int mount(const char *path, int chan_no, int dev_no) {
	return syscall3(SYS_MOUNT, path, chan_no, dev_no);
}

int umount(const char *path) { return syscall1(SYS_UMOUNT, path); }
