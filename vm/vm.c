/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/synch.h"
#include <string.h>
#include "threads/mmu.h"
#include "userprog/process.h"

static struct frame *frame_table;
static struct lock ft_lock;
void *user_start_page;
clock_t user_page_no;

/* Get next clock index */
#define next_clock(clock) (((clock) + 1) % user_page_no)
/* Convert clock index to kernal virtual address */
#define ctov(clock) ((void *)((user_start_page) + ((clock)*PGSIZE)))
/* Convert kernal virtual address to clock index */
#define vtoc(kva) ((clock_t)(pg_no((kva) - (user_start_page))))
/* Convert kernal virtual address to frame pointer */
#define vtof(kva) (frame_table + (vtoc(kva)))

static uint64_t spt_hash_func(const struct hash_elem *, void *);
static bool spt_less_func(const struct hash_elem *,
						  const struct hash_elem *, void *);
static void spt_destroy_func(struct hash_elem *, void *);

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
	if (!(frame_table = malloc(sizeof(struct frame) * user_page_no))) {
		PANIC("frame table init fail");
	}
	for (clock_t idx = 0; idx < user_page_no; ++idx) {
		(frame_table + idx)->kva = ctov(idx);
		list_init(&((frame_table + idx)->page_list));
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
#ifdef EFILESYS
		case VM_PAGE_CACHE:
			uninit_new(page, upage, init, type, aux, page_cache_initializer);
			break;
#endif
		default:
			PANIC("%d given type is abnormal", VM_TYPE(type));
			break;
		};
		page->pml4 = thread_current()->pml4;
		page->writable = writable;
		page->is_sharing = false;
		circular_init(&page->page_elem);

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
	spt_destroy_func(&page->spt_elem, NULL);
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim(void) {
	struct frame *victim;
	/* TODO: The policy for eviction is up to you. */
	static clock_t before_clock = 0;
	clock_t current_clock;
	struct page *page;
	struct list_elem *page_elem;
	bool is_acessed;
	uint64_t *pml4;

	current_clock = next_clock(before_clock);
	for (current_clock = next_clock(before_clock);
		 current_clock != before_clock;
		 current_clock = next_clock(before_clock)) {
		victim = frame_table + current_clock;
		is_acessed = false;
		for (page_elem = list_begin(&victim->page_list);
			 page_elem != list_end(&victim->page_list);
			 page_elem = list_next(page_elem)) {
			page = list_entry(page_elem, struct page, page_elem);
			pml4 = page->pml4;
			if (pml4_is_accessed(pml4, page->va)) {
				pml4_set_accessed(pml4, page->va, false);
				is_acessed = true;
			}
		}
		if (!is_acessed) {
			break;
		}
	}

	before_clock = current_clock;
	return frame_table + current_clock;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void) {
	struct frame *victim;
	struct page *page;
	struct list_elem *page_elem;
	uint64_t *pml4;

	victim = vm_get_victim();

	ASSERT(victim != NULL);

	if (list_empty(&victim->page_list)) {
		return victim;
	}
	for (page_elem = list_begin(&victim->page_list);
		 page_elem != list_end(&victim->page_list);
		 page_elem = list_next(page_elem)) {
		page = list_entry(page_elem, struct page, page_elem);
		pml4 = page->pml4;
		if (!swap_out(page)) {
			ASSERT("swap out error");
		}

		ASSERT(pml4_get_page(pml4, page->va) != NULL);

		pml4_clear_page(pml4, page->va);
	}
	// set as circular list
	circular_make(&victim->page_list);

	ASSERT(list_empty(&victim->page_list));
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* Need ft_lock before call this */
static struct frame *vm_get_frame(void) {
	struct frame *frame;
	void *kva;
	clock_t new_clock;
	/* TODO: Fill this function. */
	kva = palloc_get_page(PAL_USER);
	if (kva) {
		new_clock = vtoc(kva);
		frame = frame_table + new_clock;
		if (frame->kva != kva) {
			ASSERT(frame->kva == kva);
		}
	} else {
		frame = vm_evict_frame();
	}

	ASSERT(frame != NULL);
	ASSERT(list_empty(&frame->page_list));
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
static bool vm_handle_wp(struct page *page) {
	if (!page->writable) {
		return false;
	}

	ASSERT(page->is_sharing);

	page->is_sharing = false;
	if (!circular_is_alone(&page->page_elem)) {
		lock_acquire(&ft_lock);
		list_remove(&page->page_elem);
		lock_release(&ft_lock);

		swap_out(page);

		pml4_clear_page(page->pml4, page->va);
		circular_init(&page->page_elem);
		return vm_do_claim_page(page);
	} else {
		pml4_set_writable(page->pml4, page->va, true);
		return true;
	}
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
	if (not_present) {
		return vm_do_claim_page(page);
	}
	if (write && !vm_writable(page)) {
		return vm_handle_wp(page);
	}
	/* Only when check valid address in system call */
	return true;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
/* Modified to free kva page */
void vm_dealloc_page(struct page *page) {
	struct frame *frame;

	destroy(page);
	if (vm_on_phymem(page)) {
		frame = vtof(page->kva);

		list_remove(&page->page_elem);

		pml4_clear_page(page->pml4, page->va);
		if (list_empty(&frame->page_list)) {
			palloc_free_page(frame->kva);
		}
	}
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
	struct list_elem *cur_elem;

	ASSERT(!vm_on_phymem(page));

	/* Check called in supplemental_page_table_copy */
	if (VM_TYPE(page->operations->type) == VM_UNINIT &&
		page->is_sharing) {
		if (!swap_in(page, NULL)) {
			PANIC("I don't wan to handdle swap in fail");
		}
		frame = vtof(page->kva);
	} else {
		lock_acquire(&ft_lock);
		frame = vm_get_frame();
		lock_release(&ft_lock);

		if (!swap_in(page, frame->kva)) {
			PANIC("I don't wan to handdle swap in fail");
		}
	}

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* Traversal circular list and add in pml4 */
	if (circular_is_alone(&page->page_elem)) {
		pml4 = page->pml4;

		ASSERT(pml4_get_page(pml4, page->va) == NULL);

		if (!pml4_set_page(pml4, page->va, frame->kva, vm_writable(page))) {
			PANIC("I don't wan to write cod about pml4 fail");
		}
		list_push_back(&frame->page_list, &page->page_elem);
	} else {
		for (cur_elem = list_begin(&frame->page_list);
			 cur_elem != list_end(&frame->page_list);
			 cur_elem = list_next(cur_elem)) {
			page = list_entry(cur_elem, struct page, page_elem);
			pml4 = page->pml4;

			if (!pml4_get_page(pml4, page->va)) {
				if (!pml4_set_page(pml4, page->va, frame->kva, vm_writable(page))) {
					PANIC("I don't wan to write cod about pml4 fail");
				}
			} else if (pml4_is_writable(pml4, page->va) != vm_writable(page)) {
				pml4_set_writable(pml4, page->va, vm_writable(page));
			}
		}
	}

	return true;
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
	if (!hash_init(&spt->spt_hash, spt_hash_func, spt_less_func, NULL)) {
		PANIC("spt hash init fail");
	}
}

static bool copy_page(struct page *dst_page, void *_aux) {
	struct page *src_page = _aux;
	void *va = dst_page->va;
	struct list_elem *page_elem;

	ASSERT(dst_page->va == src_page->va);

	if (!vm_on_phymem(src_page)) {
		if (!vm_do_claim_page(src_page)) {
			PANIC("src page swap in fail");
			return false;
		}
	}

	ASSERT(src_page->kva ==
		   pml4_get_page(src_page->pml4, va));

	dst_page->kva = src_page->kva;
	src_page->is_sharing = true;

	list_insert(&src_page->page_elem, &dst_page->page_elem);
	return true;
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
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
		if (src_type == VM_FILE) {
			continue;
		}
		src_va = src_page->va;
		src_writable = vm_writable(src_page);
		if (!vm_alloc_page_with_initializer(src_type, src_va, src_writable,
											copy_page, src_page)) {
			return false;
		}
		spt_find_page(dst, src_va)->is_sharing = true;
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

/* Destroy supplemental page helper function */
void spt_destroy_func(struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry(e, struct page, spt_elem);
	vm_dealloc_page(page);
}

/* Destroy supplemental page table */
void spt_destroy(struct supplemental_page_table *spt) {
	hash_destroy(&spt->spt_hash, spt_destroy_func);
}