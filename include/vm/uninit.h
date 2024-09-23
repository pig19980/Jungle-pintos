#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer(struct page *, void *aux);

/* Uninitlialized page. The type for implementing the
 * "Lazy loading". 
 (초기화되지 않은 페이지이다.
 Lazy loading 구현 유형)*/
struct uninit_page {
	/* Initiate the contets of the page 
	(페이지의 내용을 시작한다.)*/
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	/* Initiate the struct page and maps the pa to the va 
	(구조 페이지를 시작하고 pa를 va에 매핑한다.)*/
	bool (*page_initializer)(struct page *, enum vm_type, void *kva);
};

void uninit_new(struct page *page, void *va, vm_initializer *init,
				enum vm_type type, void *aux,
				bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
