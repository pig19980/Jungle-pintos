/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include <string.h>
#include "threads/mmu.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

static bool file_init(struct page *page, void *aux);

static uint64_t mt_hash_func(const struct hash_elem *, void *);
static bool mt_less_func(const struct hash_elem *,
						 const struct hash_elem *, void *);
static void mt_destroy_func(struct hash_elem *, void *);

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
	void *kva = page->kva;
	struct vm_file_arg *arg = aux;
	struct file_page *file_page = &page->file;
	uint32_t read_bytes, zero_bytes;

	ASSERT(kva != NULL);

	file_seek(arg->file, arg->ofs);
	read_bytes = file_read(arg->file, kva, arg->read_bytes);
	zero_bytes = PGSIZE - read_bytes;
	memset(kva + read_bytes, 0, zero_bytes);

	file_page->file = arg->file;
	file_page->ofs = arg->ofs;
	file_page->read_bytes = read_bytes;

	free(aux);

	return true;
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type UNUSED, void *kva UNUSED) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	file_page->file = NULL;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool file_backed_swap_in(struct page *page, void *kva) {
	struct file_page *file_page;
	int32_t ofs;
	struct file *file;
	uint32_t read_bytes, zero_bytes;

	ASSERT(page->kva == NULL);

	file_page = &page->file;
	file = file_page->file;
	ofs = file_page->ofs;
	read_bytes = file_page->read_bytes;

	file_seek(file, ofs);
	read_bytes = file_read(file, kva, read_bytes);
	zero_bytes = PGSIZE - read_bytes;
	memset(kva + read_bytes, 0, zero_bytes);

	file_page->read_bytes = read_bytes;
	page->kva = NULL;

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool file_backed_swap_out(struct page *page) {
	struct file_page *file_page;
	int32_t ofs;
	struct file *file;
	uint32_t read_bytes;
	uint64_t *pml4;
	void *kva;

	ASSERT(page->kva != NULL);

	pml4 = thread_current()->pml4;
	kva = page->kva;

	if (pml4_is_dirty(pml4, page->va)) {
		file_page = &page->file;
		file = file_page->file;
		ofs = file_page->ofs;
		read_bytes = file_page->read_bytes;

		file_seek(file, ofs);
		file_page->read_bytes = file_write(file, kva, read_bytes);
	}

	page->kva = NULL;

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void file_backed_destroy(struct page *page) {
	struct file_page *file_page;
	int32_t ofs;
	struct file *file;
	uint32_t read_bytes;
	uint64_t *pml4;
	void *kva;

	if (!vm_on_phymem(page)) {
		return;
	}
	pml4 = thread_current()->pml4;
	kva = page->kva;
	if (pml4_is_dirty(pml4, page->va)) {
		file_page = &page->file;
		file = file_page->file;
		ofs = file_page->ofs;
		read_bytes = file_page->read_bytes;

		file_seek(file, ofs);
		file_page->read_bytes = file_write(file, kva, read_bytes);
	}
	return;
}

/* Do the mmap */
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
			  off_t offset) {
	struct vm_file_arg *file_arg;
	struct mmap *mmap = NULL;
	struct supplemental_page_table *spt;
	struct mmap_table *mt;
	void *alloc_addr;
	uint64_t alloced_idx;
	uint64_t page_count;

	// check validating input
	if (file == NULL || file == stdin || file == stdout ||
		!length || !file_length(file) || pg_ofs(offset) ||
		!addr || pg_ofs(addr)) {
		goto mmap_err;
	}
	// check over-lap
	spt = &thread_current()->spt;
	mt = &thread_current()->mt;
	page_count = pg_no(pg_round_up(length));

	for (uint64_t idx = 0; idx < page_count; ++idx) {
		void *temp_addr = addr + idx * PGSIZE;
		if (is_kernel_vaddr(temp_addr)) {
			goto mmap_err;
		}
		if (spt_find_page(spt, temp_addr)) {
			goto mmap_err;
		}
	}

	if (!(mmap = calloc(1, sizeof(struct mmap)))) {
		goto mmap_err;
	}
	if (!(mmap->file = file_reopen(file))) {
		goto mmap_err;
	}
	mmap->va = addr;
	mmap->page_count = page_count;
	if (!mt_insert_mmap(mt, mmap)) {
		PANIC("already same mmap in mt");
	}

	alloc_addr = addr;
	for (alloced_idx = 0; alloced_idx < page_count; ++alloced_idx) {
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		if (!(file_arg = malloc(sizeof(struct vm_file_arg)))) {
			alloced_idx--;
			goto mmap_err;
		}
		file_arg->file = mmap->file;
		file_arg->ofs = offset;
		file_arg->read_bytes = page_read_bytes;
		file_arg->zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, alloc_addr, writable,
											file_init, file_arg)) {
			free(file_arg);
			alloced_idx--;
			goto mmap_err;
		}

		/* Advance. */
		offset += page_read_bytes;
		length -= page_read_bytes;
		alloc_addr += PGSIZE;
	}
	return addr;
mmap_err:
	if (mmap) {
		if (mmap->file) {
			file_close(mmap->file);
		}
		free(mmap);
		for (uint32_t idx = 0; idx < alloced_idx; ++idx) {
			void *temp_addr = addr + idx * PGSIZE;
			spt_remove_page(spt, spt_find_page(spt, temp_addr));
		}
	}
	return NULL;
}

/* Do the munmap */
void do_munmap(void *addr) {
	struct supplemental_page_table *spt;
	struct mmap_table *mt;
	struct page *page;
	struct mmap *mmap;

	spt = &thread_current()->spt;
	mt = &thread_current()->mt;
	mmap = mt_find_mmap(mt, addr);
	if (!mmap) {
		return;
	}
	for (uint32_t idx = 0; idx < mmap->page_count; ++idx) {
		page = spt_find_page(spt, addr);
		ASSERT(page != NULL);
		spt_remove_page(spt, page);
		addr += PGSIZE;
	}
	mt_remove_mmap(mt, mmap);
	return;
}

/* Initialize new mmap table */
void mmap_table_init(struct mmap_table *mt) {
	if (!hash_init(&mt->mt_hash, mt_hash_func, mt_less_func, NULL)) {
		PANIC("mt hash init fail");
	}
}

/* Free the resource hold by the mmap table */
void mmap_table_kill(struct mmap_table *mt) {
	hash_clear(&mt->mt_hash, mt_destroy_func);
}

/* Find VA from mt and return mmap. On error, return NULL. */
struct mmap *mt_find_mmap(struct mmap_table *mt, void *va) {
	struct mmap key_mmap = {.va = va};
	struct hash_elem *mt_elem = hash_find(&mt->mt_hash, &key_mmap.mt_elem);
	if (!mt_elem) {
		return NULL;
	} else {
		return hash_entry(mt_elem, struct mmap, mt_elem);
	}
}

/* Insert mmap into mt with validation. */
/* If mmap is allocated, no reason to fail when inserting in hash */
bool mt_insert_mmap(struct mmap_table *mt, struct mmap *mmap) {
	return (hash_insert(&mt->mt_hash, &mmap->mt_elem) == NULL);
}

void mt_remove_mmap(struct mmap_table *mt, struct mmap *mmap) {
	if (!hash_delete(&mt->mt_hash, &mmap->mt_elem)) {
		PANIC("mmap not in mt");
	}
	file_close(mmap->file);
	free(mmap);
}

void mt_destroy(struct mmap_table *mt) {
	hash_destroy(&mt->mt_hash, mt_destroy_func);
}

static uint64_t mt_hash_func(const struct hash_elem *e, void *aux UNUSED) {
	struct mmap *mmap = hash_entry(e, struct mmap, mt_elem);
	return hash_bytes(&(mmap->va), sizeof(void *));
}

static bool mt_less_func(const struct hash_elem *a,
						 const struct hash_elem *b, void *aux UNUSED) {
	struct mmap *mmap_a = hash_entry(a, struct mmap, mt_elem);
	struct mmap *mmap_b = hash_entry(b, struct mmap, mt_elem);
	return mmap_a->va < mmap_b->va;
}

void mt_destroy_func(struct hash_elem *e, void *aux UNUSED) {
	struct mmap *mmap = hash_entry(e, struct mmap, mt_elem);
	file_close(mmap->file);
	free(mmap);
}