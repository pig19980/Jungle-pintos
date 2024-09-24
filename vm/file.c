/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"

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
	if (!hash_init(&mt->mt_hash, mt_hash_func, mt_less_func, NULL)) {
		PANIC("mt hash init fail");
	}
}

bool mmap_table_copy(struct mmap_table *dst, struct mmap_table *src) {
	struct mmap *src_mmap, *dst_mmap;
	void *src_va;
	struct file *src_file;
	struct hash_iterator current_i;
	hash_first(&current_i, &src->mt_hash);
	while (hash_next(&current_i)) {
		src_mmap = hash_entry(hash_cur(&current_i), struct mmap, mt_elem);
		src_va = src_mmap->va;
		src_file = src_mmap->file;
		if (!(dst_mmap = malloc(sizeof(struct mmap)))) {
			return false;
		}
		if (!(dst_mmap->file = file_reopen(src_file))) {
			free(dst_mmap);
			return false;
		}
		dst_mmap->va = src_va;
		if (!mt_insert_mmap(dst, dst_mmap)) {
			PANIC("already same mmap in mt");
		}
	}
	return true;
}

void mmap_table_kill(struct mmap_table *mt) {
	hash_clear(&mt->mt_hash, mt_destroy_func);
}

struct mmap *mt_find_mmap(struct mmap_table *mt, void *va) {
	struct mmap key_mmap = {.va = va};
	struct hash_elem *mt_elem = hash_find(&mt->mt_hash, &key_mmap.mt_elem);
	if (!mt_elem) {
		return NULL;
	} else {
		return hash_entry(mt_elem, struct mmap, mt_elem);
	}
}

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