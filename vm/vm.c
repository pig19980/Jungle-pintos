/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/synch.h"
#include <string.h>
#include "threads/mmu.h"
#include "userprog/process.h"

static struct hash frame_hash;
static uint64_t frame_hash_func(const struct hash_elem *, void *);
static bool frame_less_func(const struct hash_elem *,
							const struct hash_elem *, void *);
static struct lock ft_lock;

static uint64_t spt_hash_func(const struct hash_elem *, void *);
static bool spt_less_func(const struct hash_elem *,
						  const struct hash_elem *, void *);
static void spt_destroy_func(struct hash_elem *, void *);

uint64_t frame_hash_func(const struct hash_elem *e, void *aux UNUSED) {
	struct frame *frame = hash_entry(e, struct frame, ft_elem);
	return hash_bytes(&(frame->kva), sizeof(void *));
}

bool frame_less_func(const struct hash_elem *a,
					 const struct hash_elem *b, void *aux UNUSED) {

	struct frame *frame_a = hash_entry(a, struct frame, ft_elem);
	struct frame *frame_b = hash_entry(b, struct frame, ft_elem);
	return frame_a->kva < frame_b->kva;
}

uint64_t spt_hash_func(const struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry(e, struct page, spt_elem);
	return hash_bytes(&(page->va), sizeof(void *));
}

bool spt_less_func(const struct hash_elem *a,
				   const struct hash_elem *b, void *aux UNUSED) {
	struct page *page_a = hash_entry(a, struct page, spt_elem);
	struct page *page_b = hash_entry(b, struct page, spt_elem);
	return page_a->va < page_b->va;
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
	struct page *page = NULL;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		page = malloc(sizeof(struct page));
		if (!page) {
			goto err;
		}
		uninit_new(page, upage, init, type, aux, uninit_page_initializer);
		page->thread = thread_current();
		if (writable) {
			page->flags = VM_WRITABLE;
		} else {
			page->flags = 0;
		}

		/* TODO: Insert the page into the spt. */
		spt_insert_page(spt, page);
		return true;
	}
err:
	if (page) {
		free(page);
	}
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *spt,
						   void *va) {
	struct page key_page = {.va = va};
	struct hash_elem *spt_elem = hash_find(&spt->spt_hash, &key_page.spt_elem);
	if (!spt_elem) {
		return NULL;
	} else {
		return hash_entry(spt_elem, struct page, spt_elem);
	}
}

/* Insert PAGE into spt with validation. */
/* If page is allocated, no reason to fail when inserting in hash */
bool spt_insert_page(struct supplemental_page_table *spt,
					 struct page *page) {
	hash_insert(&spt->spt_hash, &page->spt_elem);
	return true;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page) {
	if (!hash_delete(&spt->spt_hash, &page->spt_elem)) {
		PANIC("page not in spt");
	}
	vm_dealloc_page(page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim(void) {
	struct frame *victim;
	/* TODO: The policy for eviction is up to you. */
	static struct hash_iterator before_i;
	struct hash_iterator current_i;
	struct page *page;
	struct list *page_list;
	bool is_accessed;
	uint64_t *pte;

	ASSERT(hash_empty(&frame_hash) == false);
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
		page_list = &victim->page_list;
		lock_acquire(&victim->page_lock);
		if (list_empty(page_list)) {
			lock_release(&victim->page_lock);
			break;
		}
		is_accessed = false;
		for (struct list_elem *page_elem = list_begin(page_list);
			 page_elem != list_end(page_list);
			 page_elem = list_next(page_elem)) {
			page = list_entry(page_elem, struct page, page_elem);
			pte = pml4e_walk(page->thread->pml4, (uint64_t)page->va, 0);
			ASSERT(pte);
			if (*pte & PTE_A) {
				*pte &= ~(uint64_t)PTE_A;
				is_accessed = true;
			}
		}
		if (!is_accessed) {
			lock_release(&victim->page_lock);
			break;
		}
		lock_release(&victim->page_lock);
	} while (hash_cur(&current_i) != hash_cur(&before_i));

	memcpy(&before_i, &current_i, sizeof(struct hash_iterator));

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void) {
	struct frame *victim = vm_get_victim();
	ASSERT(victim != NULL);
	struct list *page_list = &victim->page_list;
	struct page *page;

	lock_acquire(&victim->page_lock);
	for (struct list_elem *page_elem = list_begin(page_list);
		 page_elem != list_end(page_list);
		 page_elem = list_remove(page_elem)) {
		page = list_entry(page_elem, struct page, page_elem);
		if (!vm_swap_out(page)) {
			return NULL;
		}
	}
	lock_release(&victim->page_lock);

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame(void) {
	struct frame *frame;
	void *kva;
	struct hash_elem *old_frame_elem;
	/* TODO: Fill this function. */

	kva = palloc_get_page(PAL_USER);
	if (kva) {
		frame = malloc(sizeof(struct frame));
		ASSERT(frame != NULL);
		frame->kva = kva;
		list_init(&frame->page_list);
		lock_init(&frame->page_lock);
		old_frame_elem = hash_insert(&frame_hash, &frame->ft_elem);
		ASSERT(old_frame_elem == NULL);
	} else {
		frame = vm_evict_frame();
	}

	ASSERT(frame != NULL);
	ASSERT(list_empty(&frame->page_list) == true);
	return frame;
}

static struct frame *vm_check_sharing_and_get_frame(struct page *page) {
	struct frame *frame;
	if (vm_sharing(page)) {
		if (!page->sharing_page->frame) {
			lock_acquire(&ft_lock);
			page->sharing_page->frame = vm_get_frame();
			lock_release(&ft_lock);
		}
		return page->sharing_page->frame;
	} else {
		lock_acquire(&ft_lock);
		frame = vm_get_frame();
		lock_release(&ft_lock);
		return frame;
	}
}

/* Growing the stack. */
static void vm_stack_growth(void *addr UNUSED) {}

/* Handle the fault on write_protected page */
static bool vm_handle_wp(struct page *page UNUSED) {}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f, void *addr,
						 bool user, bool write,
						 bool not_present) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = NULL, *real_page;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (user && is_kernel_vaddr(pg_round_down(addr))) {
		return false;
	}
	page = spt_find_page(spt, pg_round_down(addr));
	if (page == NULL) {
		return false;
	}

	if (vm_sharing(page)) {
		real_page = page->sharing_page;
	} else {
		real_page = page;
	}

	if (write && !vm_writable(real_page)) {
		return false;
	}

	if (not_present) {
		return vm_do_claim_page(page);
	}
	if (vm_sharing(page) &&
		write && vm_writable(real_page)) {
		return vm_split_page(page);
	} else {
		return vm_do_claim_page(page);
	}
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page) {
	vm_destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va) {
	struct page *page;
	struct supplemental_page_table *spt;

	spt = &thread_current()->spt;
	page = spt_find_page(spt, va);
	if (!page) {
		return false;
	}
	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page(struct page *page) {
	struct frame *frame = vm_check_sharing_and_get_frame(page);

	/* Set links */
	lock_acquire(&frame->page_lock);
	list_push_back(&frame->page_list, &page->page_elem);
	lock_release(&frame->page_lock);
	if (vm_sharing(page)) {
		page->sharing_page->frame = frame;
	} else {
		page->frame = frame;
	}
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	return vm_swap_in(page, frame->kva);
}

/* Considering sharing and swap in */
bool vm_swap_in(struct page *page, void *kva) {
	uint64_t *pml4;
	bool writable;
	pml4 = page->thread->pml4;
	ASSERT(pml4_get_page(pml4, page->va) == NULL);
	writable = vm_sharing(page) ? false : vm_writable(page);
	if (!pml4_set_page(pml4, page->va, kva, writable)) {
		return false;
	}
	if (vm_sharing(page)) {
		if (vm_on_phymem(page->sharing_page)) {
			return true;
		} else {
			return swap_in(page->sharing_page, kva);
		}
	} else {
		return swap_in(page, kva);
	}
}

/* Considering sharing and swap out */
bool vm_swap_out(struct page *page) {
	uint64_t *pml4;
	uint64_t *pte;
	bool swap_out_ret;
	if (page->sharing_page) {
		if (!vm_on_phymem(page->sharing_page)) {
			swap_out_ret = swap_out(page->sharing_page);
		} else {
			swap_out_ret = true;
		}
	} else {
		swap_out_ret = swap_out(page);
	}
	if (swap_out_ret) {
		pml4 = page->thread->pml4;
		pte = pml4e_walk(pml4, (uint64_t)page->va, false);
		ASSERT(pte != NULL && (*pte & PTE_P) != 0);
		pml4_clear_page(pml4, page->va);
		return true;
	} else {
		return false;
	}
}

/* Considering sharing and destroy */
void vm_destroy(struct page *page) {
	if (vm_sharing(page)) {
		struct page *sharing_page = page->sharing_page;
		lock_acquire(&sharing_page->sharing_lock);
		list_remove(&page->sharing_elem);
		if (list_empty(&sharing_page->sharing_list)) {
			lock_release(&sharing_page->sharing_lock);
			destroy(sharing_page);
			free(sharing_page);
		} else {
			lock_release(&sharing_page->sharing_lock);
		}
	} else {
		destroy(page);
	}
}

/* Split page when copied page is written */
bool vm_split_page(struct page *page UNUSED) {
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
void supplemental_page_table_kill(struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->spt_hash, spt_destroy_func);
}

void spt_destroy_func(struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry(e, struct page, spt_elem);
	vm_dealloc_page(page);
}

void spt_destroy(struct supplemental_page_table *spt) {
	hash_destroy(&spt->spt_hash, spt_destroy_func);
}