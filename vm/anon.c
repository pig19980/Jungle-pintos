/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

static void swap_write(disk_sector_t sec_no, const void *buffer);
static void swap_read(disk_sector_t sec_no, void *buffer);

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
	ASSERT(page->kva == kva);
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->sec_no = BITMAP_ERROR;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	ASSERT(bitmap_all(swap_bitmap, anon_page->sec_no, SEC_WRITE_CNT));
	ASSERT(page->kva == NULL);

	swap_read(anon_page->sec_no, kva);
	lock_acquire(&swap_lock);
	bitmap_set_multiple(swap_bitmap, anon_page->sec_no, SEC_WRITE_CNT, false);
	lock_release(&swap_lock);
	anon_page->sec_no = BITMAP_ERROR;
	page->kva = kva;

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) {
	struct anon_page *anon_page = &page->anon;

	ASSERT(anon_page->sec_no == BITMAP_ERROR);
	ASSERT(page->kva != NULL);

	lock_acquire(&swap_lock);
	anon_page->sec_no = bitmap_scan_and_flip(swap_bitmap, 0, SEC_WRITE_CNT, false);
	lock_release(&swap_lock);
	if (anon_page->sec_no == BITMAP_ERROR) {
		return false;
	}
	swap_write(anon_page->sec_no, page->kva);
	page->kva = NULL;

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if (!vm_on_phymem(page)) {
		ASSERT(anon_page->sec_no != BITMAP_ERROR);
		ASSERT(bitmap_all(swap_bitmap, anon_page->sec_no, SEC_WRITE_CNT));

		lock_acquire(&swap_lock);
		bitmap_set_multiple(swap_bitmap, anon_page->sec_no, SEC_WRITE_CNT, false);
		lock_release(&swap_lock);
		anon_page->sec_no = BITMAP_ERROR;
	}
}

static void swap_write(disk_sector_t sec_no, const void *buffer) {
	ASSERT(sec_no + SEC_WRITE_CNT <= sec_cnt);
	for (int i = 0; i < SEC_WRITE_CNT; ++i) {
		disk_write(swap_disk, sec_no + i, buffer + DISK_SECTOR_SIZE * i);
	}
}

static void swap_read(disk_sector_t sec_no, void *buffer) {
	ASSERT(sec_no + SEC_WRITE_CNT <= sec_cnt);
	for (int i = 0; i < SEC_WRITE_CNT; ++i) {
		disk_read(swap_disk, sec_no + i, buffer + DISK_SECTOR_SIZE * i);
	}
}