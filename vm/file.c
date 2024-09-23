/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

static bool file_init(struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void) {}

/* Called when swap in from VM_UNINIT state */
bool file_init(struct page *page, void *aux) {
	return false;
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool file_backed_swap_in(struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool file_backed_swap_out(struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void file_backed_destroy(struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
			  off_t offset) {
	struct vm_file_arg *file_arg;

	if (file == NULL || file == stdin || file == stdout) {
		return NULL;
	}
	// need more address check....
	file_arg = malloc(sizeof(struct vm_file_arg));
	if (vm_alloc_page_with_initializer(VM_FILE, addr, writable, file_init, file_arg)) {
		return addr;
	} else {
		return NULL;
	}
}

/* Do the munmap */
void do_munmap(void *addr) {
	return;
}

void mmap_table_init(struct mmap_table *mt) {
	return;
}

bool mmap_table_copy(struct mmap_table *dst,
					 struct mmap_table *src) {
	return false;
}

void mmap_table_kill(struct mmap_table *mt) {
	return;
}

struct mmap *mt_find_mmap(struct mmap_table *mt, void *va) {
	return NULL;
}

bool mt_insert_mmap(struct mmap_table *mt, struct mmap *page) {
	return false;
}

void mt_remove_mmap(struct mmap_table *mt, struct mmap *page) {
	return;
}

void mt_destroy(struct mmap_table *mt) {
	return;
}