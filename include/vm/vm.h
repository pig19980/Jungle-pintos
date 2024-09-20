#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include <hash.h>

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

enum page_flags {
	/* page is writable */
	VM_WRITABLE = 1,
	/* page is sharing frame with other page */
	VM_SHARING = 2,
	/* page is swap in status */
	VM_ON_PHYMEM = 4,
};

#include "devices/disk.h"
#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif
#include "threads/synch.h"

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type)&7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va; /* Address in terms of user space */
	union {
		struct frame *frame; /* Back reference for frame */
		struct page *sharing_page;
	};

	enum page_flags flags;
	struct thread *thread;

	struct list sharing_list;	  /* List of page using same frame*/
	struct list_elem sharing_elem; /* Using same frame if sharing_page is same */
	struct lock sharing_lock;	  /* Lock for acessing sharing_list*/

	struct hash_elem spt_elem;
	struct list_elem page_elem; /* Frame get page by this elem*/
	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

#define vm_writable(page) ((((page)->flags) & VM_WRITABLE) != 0)
#define vm_sharing(page) ((((page)->flags) & VM_SHARING) != 0)
#define vm_on_phymem(page) ((((page)->flags) & VM_ON_PHYMEM) != 0)

/* The representation of "frame" */
struct frame {
	void *kva;
	struct list page_list;
	struct lock page_lock;
	struct hash_elem ft_elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in)(struct page *, void *);
	bool (*swap_out)(struct page *);
	void (*destroy)(struct page *);
	enum vm_type type;
};

bool vm_swap_in(struct page *page, void *kva);
bool vm_swap_out(struct page *page);
void vm_destroy(struct page *page);

bool vm_split_page(struct page *page);

#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)
#define destroy(page)                \
	if ((page)->operations->destroy) \
	(page)->operations->destroy(page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
struct supplemental_page_table {
	struct hash spt_hash;
};

#include "threads/thread.h"
void supplemental_page_table_init(struct supplemental_page_table *spt);
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
								  struct supplemental_page_table *src);
void supplemental_page_table_kill(struct supplemental_page_table *spt);
struct page *spt_find_page(struct supplemental_page_table *spt, void *va);
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page);
void spt_remove_page(struct supplemental_page_table *spt, struct page *page);

void spt_destroy(struct supplemental_page_table *spt);

void vm_init(void);
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user,
						 bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
									bool writable, vm_initializer *init,
									void *aux);
void vm_dealloc_page(struct page *page);
bool vm_claim_page(void *va);
enum vm_type page_get_type(struct page *page);

struct vm_alloc_with_file_arg {
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};

#endif /* VM_VM_H */
