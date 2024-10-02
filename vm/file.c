/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "lib/string.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,	// 페이지를 삭제하는 함수이다.
	.type = VM_FILE,
};

struct file_aux {
	uint32_t read_bytes;
	uint32_t zero_bytes;
	struct file *file;
	off_t ofs;
};

/* The initializer of file vm 
(파일 지원 페이지 하위시스템을 초기화한다. 이 기능에서는
파일 백업 페이지와 관련된 모든 것을 설정할 수 있다.)*/
void vm_file_init(void) {}

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

static bool lazy_file_load_segment(struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct file_aux *file_aux = aux;
	void *kva = page -> frame -> kva;

	uint32_t lazy_read_bytes = file_aux -> read_bytes;
	uint32_t lazy_zero_bytes = file_aux -> zero_bytes;
	struct file *lazy_file = file_aux -> file;
	off_t lazy_ofs = file_aux -> ofs;

	file_seek(lazy_file, lazy_ofs);

	if (file_read(lazy_file, kva, lazy_read_bytes) != (int)lazy_read_bytes) {
		memset(kva, aux, lazy_read_bytes);
		return false;
	}
	
	memset(kva + lazy_read_bytes, 0, lazy_zero_bytes);
	free(aux);
	return true;
}

/* Do the mmap */
void *do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset) {

	struct file *f = file_reopen(file);
	// length가 read_bytes인데 다른경우가 있는데 그것을 cut할 수 있게 짜라
	while (length > 0) {
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_aux *aux = (struct file_aux*)malloc(sizeof(struct file_aux));
		if (!aux) {
			return false;
		}
		aux -> read_bytes = page_read_bytes;
		aux -> zero_bytes = page_zero_bytes;
		aux -> file = f;
		aux -> ofs = offset;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_file_load_segment, aux))
			return NULL;

		length -= page_read_bytes;
		// zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	}

/* Do the munmap */
void do_munmap(void *addr) { }
