/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

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
 * `vm_alloc_page`. 
 * (이 함수는 페이지 구조체를 할당하고 페이지 타입에 맞는 적절한 초기화 함수를
 * 세팅함으로써 새로운 페이지를 초기화를 한다. 그리고 유저 프로그램으로 제어권을
 * 넘긴다.)*/
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
									bool writable, vm_initializer *init,
									void *aux) {
	
	bool success = false;
	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not.
	(upage가 이미 점유되어 있는지 확인한다.) */
	if (spt_find_page(spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new.
		 * (TODO: 페이지를 생성하고, VM 유형에 따라 초기화 함수를 가져온 후,
         * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 생성하세요.
         * TODO: uninit_new 호출 후 필드를 수정해야 합니다.) 
		 * spt 안에 삽입하는 것은 왜 안해 ? 
		 * success TRUE로 리턴을 안해준다*/
		struct page *p = palloc_get_page(PAL_USER);
		if (type == VM_ANON) 
			uninit_new(p, p -> va, init, VM_UNINIT, aux, anon_initializer);
		else if (type == VM_FILE)
			uninit_new(p, p -> va, init, VM_UNINIT, aux, file_backed_initializer);



		/* TODO: Insert the page into the spt.
		(페이지를 spt에 삽입하세요.) */
	}
	return success = true;
err:
	return success;
}

/* Find VA from spt and return page. On error, return NULL. 
(인자로 넘겨진 보조 페이지 테이블(spt) 에서로부터 가상 주소(va)와 
대응되는 페이지 구조체를 찾아서 반환한다. 실패했을 경우 NULL을 반환한다.)*/
struct page *spt_find_page(struct supplemental_page_table *spt UNUSED,
						   void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */

	return page;
}

/* Insert PAGE into spt with validation.
(인자로 주어진 보조 페이지 테이블에 페이지 구조체를 삽입한다.
이 함수에서 주어진 보충 테이블에서 가상 주소가 존재하지 않는지
검사해야 한다.) */
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
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void) {
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.
 * (palloc_get_page()함수를 호출함으로써 메모리 풀에서 새로운 물리메모리 페이지를
 * 가져온다. 유저 메모리 풀에서 페이지를 성공적으로 가져오면, 프레임을 할당하고 
 * 프레임 구조체의 멤버들을 초기화한 후 해당 프레임을 반환한다.)*/
static struct frame *vm_get_frame(void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	//PANIC("todo")
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void vm_stack_growth(void *addr UNUSED) {}

/* Handle the fault on write_protected page */
static bool vm_handle_wp(struct page *page UNUSED) {}

/* Return true on success 
(유효한 페이지 폴트인지를 우선 검사한다.)*/
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr  UNUSED,
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

/* Claim the page that allocate on VA. 
(인자로 주어진 va에 페이지를 할당하고, 해당
페이지에 프레임을 할당한다. 당신은 우선 한
페이지를 얻어야 하고 그 이후에 해당 페이지를
인자로 갖는 vm_do_claim_page라는 함수를 호출
해야 한다.)*/
bool vm_claim_page(void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu.
(인자로 주어진 page에 물리 메모리 프레임을 할당한다. 당신은 먼저
vm_get_frame함수를 호출함으로써 프레임 하나를 얻는다.(이 부분은 
스켈레톤 코드에 구현되어있다.) 그 이후 MMU를 세팅해야 하는데,
이는 가상 주소와 물리 주소를 매핑한 정보를 페이지 테이블에 추가해야
한다는 것을 의미함.) */
static bool vm_do_claim_page(struct page *page) {
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table 
(보조 페이지 테이블을 초기화합니다. 보조 페이지 테이블을 어떤 자료 구조로 구현할지 선택하세요.
userprog/process.c의 initd 함수로 새로운 프로세스가 시작하거나 process.c의 __do_fork로 자식
프로세스가 생성될 때 위의 함수가 호출된다.)*/
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {}

/* Copy supplemental page table from src to dst 
(src부터 dst까지 supplemental page table을 복사하세요.
이것은 자식이 부모의 실행 context를 상속할 필요가 있을 때 사용된다.
(예 - fork()) src의 supplemental page table을 반복하면서
dst의 supplemental page table의 엔트리의 정확한 복사본을
만드세요. 당신은 초기화되지 않은(uninit)페이지를 할당하고
그것들을 바로 요청할 필요가 있을 것이다.)*/
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED) {}

/* Free the resource hold by the supplemental page table
(supplemental page table에 의해 유지되던 모든 자원들을
free 한다. 이 함수는 process가 exit할 때 (userprog/process.c의 process_exit()
호출된다. 당신은 페이지 엔트리를 반복하면서 테이블의 페이지에 destroy(page)를
호출하여야 한다. 당신은 이 함수에서 실제 페이지 테이블(pml4)와 물리주소
(palloc된 메모리)에 대해 걱정할 필요가 없다. supplemental page table이
정리되어지고 나서, 호출자가 그것들을 정리할 것이다.)) */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
