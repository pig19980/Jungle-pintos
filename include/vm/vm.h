#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

//project 3
#include <hash.h>

enum vm_type {
	/* page not initialized (페이지가 초기화되지 않음)*/
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page(파일과 관련 없는 페이지 일명 익명 페이지) */
	VM_ANON = 1,
	/* page that realated to the file (파일로 구현 된 페이지.)*/
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

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type)&7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;			 /* Address in terms of user space (사용자 공간 단위로 주소를 지정한다.)*/
	struct frame *frame; /* Back reference for frame(물리 메모리 페이지를 가리킴) */

	/* Your implementation */

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union 
	 (union 자료형은 하나의 메모리 영역에 다른 타입의 데이터를 저장하는 것을
	 허용하는 특별한 자료형이다. 하나의 union은 여러 개의 멤버를 가질 수 있지만,
	 한 번에 멤버 중 하나의 값을 가질 수 있다.
	 즉, 같은 메모리 영역을 여러 개의 변수들이 공유할 수 있게 하는 기능.
	 메모리 절약.)*/
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;	//커널 가상 주소
	struct page *page;	// 페이지 구조체를 담기 위한 멤버
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

#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)
#define destroy(page)                \
	if ((page)->operations->destroy) \
	(page)->operations->destroy(page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. 
 * (현재 프로세스의 메모리 공간을 나타낸다.
 * 이 구조에 대한 특정 설계를 따르도록 강요하고 싶지 않습니다.
 * 이를 위한 모든 디자인은 여러분에게 달려 있습니다.)*/
struct supplemental_page_table {
	struct frame *frame;
	struct page *page;
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

#endif /* VM_VM_H */
