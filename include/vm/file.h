#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	struct file *file;
	int32_t ofs;
	uint32_t read_bytes;
};

struct mmap_table {
	struct hash mt_hash;
};

struct mmap {
	void *va;
	struct file *file;
	uint32_t page_count;
	struct hash_elem mt_elem;
};

void vm_file_init(void);
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
			  off_t offset);
void do_munmap(void *va);

void mmap_table_init(struct mmap_table *mt);
void mmap_table_kill(struct mmap_table *mt);
struct mmap *mt_find_mmap(struct mmap_table *mt, void *va);
bool mt_insert_mmap(struct mmap_table *mt, struct mmap *page);
void mt_remove_mmap(struct mmap_table *mt, struct mmap *page);

void mt_destroy(struct mmap_table *mt);
#endif
