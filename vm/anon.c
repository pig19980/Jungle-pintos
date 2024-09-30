/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages 
(익명 페이지 서브시스템(하위 시스템)을 초기화한다.
이 함수에서, 당신은 익명 페이지에 관련된 어떤 것이든
설정할 수 있다.)*/
void vm_anon_init(void) {
	/* TODO: Set up the swap_disk. */
	// swap_disk = NULL;
	swap_disk = disk_get(1,1);
}

/* Initialize the file mapping 
(이 함수는 처음으로 page -> operation에 있는 익명 페이지에 대한 핸들러를
설정하여 준다. 당신은 현재는 비어있는 구조체인 anon_page의 정보들을 업데이트
할 필요가 있을 것이다. 이 함수는 익명 페이지를 초기화하는데 사용된다. 예 - VM_ANON)*/
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
