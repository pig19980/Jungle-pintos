/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/synch.h"
#include <string.h>
#include "threads/mmu.h"

static struct hash frame_hash;
static uint64_t frame_hash_func(const struct hash_elem *, void *);
static bool frame_less_func(const struct hash_elem *,
							const struct hash_elem *, void *);
static struct lock ft_lock;

static uint64_t spt_hash_func(const struct hash_elem *, void *);
static bool spt_less_func(const struct hash_elem *,
						  const struct hash_elem *, void *);

uint64_t frame_hash_func(const struct hash_elem *e, void *aux) {
	return hash_bytes(&(hash_entry(e, struct frame, ft_elem)->kva),
					  sizeof(void *));
}

bool frame_less_func(const struct hash_elem *a,
					 const struct hash_elem *b, void *aux UNUSED) {
	return hash_entry(a, struct frame, ft_elem)->kva <
		   hash_entry(b, struct frame, ft_elem)->kva;
}

uint64_t spt_hash_func(const struct hash_elem *e, void *aux) {
	return hash_bytes(&(hash_entry(e, struct page, spt_elem)->va),
					  sizeof(void *));
}

bool spt_less_func(const struct hash_elem *a,
				   const struct hash_elem *b, void *aux UNUSED) {
	return hash_entry(a, struct page, spt_elem)->va <
		   hash_entry(b, struct page, spt_elem)->va;
}

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void) {
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	if (!hash_init(&frame_hash, frame_hash_func, frame_less_func, NULL)) {
		PANIC("frame hash init fail");
	}
	lock_init(&ft_lock);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type page_get_type(struct page *page) {
	int ty = VM_TYPE(page->operations->type);
	switch (ty) {
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
									bool writable, vm_initializer *init,
									void *aux) {

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *spt UNUSED,
						   void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */

	return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim(void) {
	struct frame *victim;
	/* TODO: The policy for eviction is up to you. */
	static struct hash_iterator before_i;
	struct hash_iterator current_i;
	struct page *page;
	uint64_t *pte;

	lock_acquire(&ft_lock);
	ASSERT(!hash_empty(&frame_hash));
	if (!hash_cur(&before_i)) {
		hash_first(&before_i, &frame_hash);
	}
	memcpy(&current_i, &before_i, sizeof(struct hash_iterator));
	do {
		hash_next(&current_i);
		if (!hash_cur(&current_i)) {
			hash_first(&current_i, &frame_hash);
		}
		victim = hash_entry(hash_cur(&current_i), struct frame, ft_elem);
		page = victim->page;
		// I don't know this is right
		if (!page) {
			continue;
		}
		//////////
		pte = pml4e_walk(page->process->thread.pml4, (uint64_t)page->va, 0);
		ASSERT(pte);
		if (*pte & PTE_A) {
			*pte &= ~(uint64_t)PTE_A;
		} else {
			break;
		}
	} while (hash_cur(&current_i) != hash_cur(&before_i));

	memcpy(&before_i, &current_i, sizeof(struct hash_iterator));
	lock_release(&ft_lock);

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void) {
	struct frame *victim = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	ASSERT(victim != NULL);
	bool swap_ret = swap_out(victim->page);
	ASSERT(swap_ret == true);
	victim->page = NULL;

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame(void) {
	struct frame *frame;
	struct page *page;
	void *kva;
	struct hash_elem *old_frame_elem;
	/* TODO: Fill this function. */

	kva = palloc_get_page(PAL_USER);
	if (kva) {
		frame = malloc(sizeof(struct frame));
		ASSERT(frame != NULL);
		frame->kva = kva;
		frame->page = NULL;
		lock_acquire(&ft_lock);
		old_frame_elem = hash_insert(&frame_hash, &frame->ft_elem);
		lock_release(&ft_lock);
	} else {
		frame = vm_evict_frame();
	}

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void vm_stack_growth(void *addr UNUSED) {}

/* Handle the fault on write_protected page */
static bool vm_handle_wp(struct page *page UNUSED) {}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED,
						 bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page) {
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page(struct page *page) {
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
	if (!hash_init(&spt->spt_hash, spt_hash_func, spt_less_func, NULL)) {
		PANIC("spt hash init fail");
	}
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED) {}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
