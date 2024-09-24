/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/synch.h"
#include <string.h>
#include "threads/mmu.h"
#include "userprog/process.h"

static struct hash ft_hash;
static uint64_t ft_hash_func(const struct hash_elem *, void *);
static bool ft_less_func(const struct hash_elem *,
						 const struct hash_elem *, void *);
static struct lock ft_lock;

static uint64_t spt_hash_func(const struct hash_elem *, void *);
static bool spt_less_func(const struct hash_elem *,
						  const struct hash_elem *, void *);
static void spt_destroy_func(struct hash_elem *, void *);

static uint64_t ft_hash_func(const struct hash_elem *e, void *aux UNUSED) {
	struct frame *frame = hash_entry(e, struct frame, ft_elem);
	return hash_bytes(&(frame->kva), sizeof(void *));
}

static bool ft_less_func(const struct hash_elem *a,
						 const struct hash_elem *b, void *aux UNUSED) {

	struct frame *frame_a = hash_entry(a, struct frame, ft_elem);
	struct frame *frame_b = hash_entry(b, struct frame, ft_elem);
	return frame_a->kva < frame_b->kva;
}

static uint64_t spt_hash_func(const struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry(e, struct page, spt_elem);
	return hash_bytes(&(page->va), sizeof(void *));
}

static bool spt_less_func(const struct hash_elem *a,
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
	if (!hash_init(&ft_hash, ft_hash_func, ft_less_func, NULL)) {
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
		switch (VM_TYPE(type)) {
		case VM_ANON:
			uninit_new(page, upage, init, type, aux, anon_initializer);
			break;
		case VM_FILE:
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
			break;
		// case VM_PAGE_CACHE:
		// 	uninit_new(page, upage, init, type, aux, page_cache_initializer);
		// 	break;
		default:
			PANIC("%d given type is abnormal", VM_TYPE(type));
			break;
		};
		page->pml4 = thread_current()->pml4;
		page->frame = NULL;
		lock_init(&page->page_lock);
		if (writable) {
			page->flags = VM_WRITABLE;
		} else {
			page->flags = 0;
		}

		/* TODO: Insert the page into the spt. */
		if (!spt_insert_page(spt, page)) {
			PANIC("already same page in spt");
		}
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
	return (hash_insert(&spt->spt_hash, &page->spt_elem) == NULL);
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
	struct hash_iterator current_i;
	struct page *page;
	uint64_t *pml4;

	ASSERT(hash_empty(&ft_hash) == false);
	hash_first(&current_i, &ft_hash);
	while (hash_next(&current_i)) {
		victim = hash_entry(hash_cur(&current_i), struct frame, ft_elem);
		page = victim->page;
		ASSERT(page != NULL);
		pml4 = page->pml4;
		if (pml4_is_accessed(pml4, page->va)) {
			pml4_set_accessed(pml4, page->va, false);
		} else {
			return victim;
		}
	};

	hash_first(&current_i, &ft_hash);
	hash_next(&current_i);
	return victim = hash_entry(hash_cur(&current_i), struct frame, ft_elem);
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void) {
	struct frame *victim;
	struct page *page;
	uint64_t *pml4;

	victim = vm_get_victim();
	ASSERT(victim != NULL);

	page = victim->page;
	if (!page) {
		return victim;
	}
	pml4 = page->pml4;
	if (!swap_out(page)) {
		return NULL;
	}

	ASSERT(pml4_get_page(pml4, page->va) != NULL);

	lock_acquire(&page->page_lock);
	page->flags &= ~VM_ON_PHYMEM;
	victim->page = NULL;
	page->frame = NULL;
	lock_release(&page->page_lock);

	pml4_clear_page(pml4, page->va);

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame(void) {
	struct frame *frame;
	void *kva;
	struct hash_elem *old_frame_elem = NULL;
	/* TODO: Fill this function. */

	lock_acquire(&ft_lock);
	kva = palloc_get_page(PAL_USER);
	if (kva) {
		frame = malloc(sizeof(struct frame));
		ASSERT(frame != NULL);
		frame->kva = kva;
		frame->page = NULL;
		old_frame_elem = hash_insert(&ft_hash, &frame->ft_elem);
		ASSERT(old_frame_elem == NULL);
	} else {
		frame = vm_evict_frame();
	}
	lock_release(&ft_lock);

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void vm_stack_growth(void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	addr = pg_round_down(addr);
	while (!spt_find_page(spt, addr)) {
		if (!vm_alloc_page(VM_ANON, addr, true)) {
			exit_with_exit_status(-1);
		}
		addr += PGSIZE;
	}
}

/* Handle the fault on write_protected page */
static bool vm_handle_wp(struct page *page UNUSED) {
	return false;
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f, void *addr,
						 bool user, bool write,
						 bool not_present) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (user && is_kernel_vaddr(addr)) {
		return false;
	}
	page = spt_find_page(spt, pg_round_down(addr));
	if (page == NULL) {
		if (f->rsp - 8 <= (uintptr_t)addr) {
			vm_stack_growth(addr);
			return true;
		}
		return false;
	}
	if (write && !vm_writable(page)) {
		return vm_handle_wp(page);
	}
	if (not_present) {
		return vm_do_claim_page(page);
	}
	/* Only when check valid address in system call */
	return true;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page) {
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va) {
	struct page *page;
	struct supplemental_page_table *spt;

	spt = &thread_current()->spt;
	page = spt_find_page(spt, pg_round_down(va));
	if (!page) {
		return false;
	}

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page(struct page *page) {
	struct frame *frame;
	uint64_t *pml4;
	bool sucess;

	frame = vm_get_frame();
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4 = page->pml4;
	ASSERT(pml4_get_page(pml4, page->va) == NULL);
	if (!pml4_set_page(pml4, page->va, frame->kva, vm_writable(page))) {
		return false;
	}

	ASSERT(!vm_on_phymem(page));

	lock_acquire(&page->page_lock);
	/* Set links */
	frame->page = page;
	page->frame = frame;
	if (swap_in(page, frame->kva)) {
		page->flags |= VM_ON_PHYMEM;
		sucess = true;
	} else {
		page->frame = NULL;
		frame->page = NULL;
		sucess = false;
	}
	lock_release(&page->page_lock);
	return sucess;
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
	if (!hash_init(&spt->spt_hash, spt_hash_func, spt_less_func, NULL)) {
		PANIC("spt hash init fail");
	}
}

static bool copy_page(struct page *dst_page, void *_aux) {
	struct page *src_page = _aux;
	void *kva = dst_page->frame->kva;
	bool success = false;
	ASSERT(dst_page->va == src_page->va);
	lock_acquire(&src_page->page_lock);
	if (vm_on_phymem(src_page)) {
		ASSERT(src_page->frame->kva ==
			   pml4_get_page(src_page->pml4, src_page->va));

		memcpy(kva, src_page->frame->kva, PGSIZE);
		success = true;
	} else {
		src_page->frame = dst_page->frame;
		if (swap_in(src_page, kva) && swap_out(src_page)) {
			success = true;
		} else {
			success = false;
		}
		src_page->frame = NULL;
	}
	lock_release(&src_page->page_lock);

	if (success) {
		return true;
	} else {
		return false;
	}
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src) {
	struct page *src_page;
	enum vm_type src_type;
	void *src_va;
	bool src_writable;
	struct hash_iterator current_i;
	hash_first(&current_i, &src->spt_hash);
	while (hash_next(&current_i)) {
		src_page = hash_entry(hash_cur(&current_i), struct page, spt_elem);
		src_type = page_get_type(src_page);
		src_va = src_page->va;
		src_writable = vm_writable(src_page);
		if (!vm_alloc_page_with_initializer(src_type, src_va, src_writable,
											copy_page, src_page)) {
			return false;
		}
		if (!vm_claim_page(src_va)) {
			return false;
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	lock_acquire(&ft_lock);
	hash_clear(&spt->spt_hash, spt_destroy_func);
	lock_release(&ft_lock);
}

void spt_destroy_func(struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry(e, struct page, spt_elem);
	struct frame *frame = page->frame;
	if (vm_on_phymem(page)) {
		frame->page = NULL;
		pml4_clear_page(page->pml4, page->va);
		if (frame->page == NULL) {
			palloc_free_page(frame->kva);
			hash_delete(&ft_hash, &frame->ft_elem);
			free(frame);
		}
	}
	vm_dealloc_page(page);
}

void spt_destroy(struct supplemental_page_table *spt) {
	hash_destroy(&spt->spt_hash, spt_destroy_func);
}