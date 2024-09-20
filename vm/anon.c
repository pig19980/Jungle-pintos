/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "threads/malloc.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

static struct bitmap *swap_bitmap;
static struct lock swap_lock;
static disk_sector_t sec_cnt;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	sec_cnt = disk_size(swap_disk);
	swap_bitmap = bitmap_create(sec_cnt);
	lock_init(&swap_lock);
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	ASSERT(type == VM_ANON);
	ASSERT(page->frame->kva == kva);
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->sec_no = BITMAP_ERROR;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	uint64_t *pml4 = page->thread->pml4;
	disk_read(swap_disk, anon_page->sec_no, kva);
	lock_acquire(&swap_lock);
	ASSERT(bitmap_test(swap_bitmap, anon_page->sec_no) == true);
	bitmap_reset(swap_bitmap, anon_page->sec_no);
	lock_release(&swap_lock);
	anon_page->sec_no = BITMAP_ERROR;
	return (pml4_get_page(pml4, page->va) == NULL &&
			pml4_set_page(pml4, page->va, kva, page->writable));
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) {
	struct anon_page *anon_page = &page->anon;
	lock_acquire(&swap_lock);
	anon_page->sec_no = bitmap_scan_and_flip(swap_bitmap, 0, sec_cnt, false);
	lock_release(&swap_lock);
	if (anon_page->sec_no == BITMAP_ERROR) {
		return false;
	}
	disk_write(swap_disk, anon_page->sec_no, page->frame->kva);
	page->frame = NULL;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if (anon_page->sec_no != BITMAP_ERROR) {
		lock_acquire(&swap_lock);
		ASSERT(bitmap_test(swap_bitmap, anon_page->sec_no) == true);
		bitmap_reset(swap_bitmap, anon_page->sec_no);
		lock_release(&swap_lock);
	}
}
