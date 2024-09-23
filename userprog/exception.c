#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "intrinsic.h"
#include "userprog/process.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill(struct intr_frame *);
static void page_fault(struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void exception_init(void) {
	/* These exceptions can be raised explicitly by a user program,
	   e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
	   we set DPL==3, meaning that user programs are allowed to
	   invoke them via these instructions. */
	intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
	intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
	intr_register_int(5, 3, INTR_ON, kill,
					  "#BR BOUND Range Exceeded Exception");

	/* These exceptions have DPL==0, preventing user processes from
	   invoking them via the INT instruction.  They can still be
	   caused indirectly, e.g. #DE can be caused by dividing by
	   0.  */
	intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
	intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
	intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
	intr_register_int(7, 0, INTR_ON, kill,
					  "#NM Device Not Available Exception");
	intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
	intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
	intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
	intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
	intr_register_int(19, 0, INTR_ON, kill,
					  "#XF SIMD Floating-Point Exception");

	/* Most exceptions can be handled with interrupts turned on.
	   We need to disable interrupts for page faults because the
	   fault address is stored in CR2 and needs to be preserved. */
	intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void exception_print_stats(void) {
	printf("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void kill(struct intr_frame *f) {
	/* This interrupt is one (probably) caused by a user process.
	   For example, the process might have tried to access unmapped
	   virtual memory (a page fault).  For now, we simply kill the
	   user process.  Later, we'll want to handle page faults in
	   the kernel.  Real Unix-like operating systems pass most
	   exceptions back to the process via signals, but we don't
	   implement them. */

	/* The interrupt frame's code segment value tells us where the
	   exception originated. */
	switch (f->cs) {
	case SEL_UCSEG:
		/* User's code segment, so it's a user exception, as we
		   expected.  Kill the user process.  */
		printf("%s: dying due to interrupt %#04llx (%s).\n", thread_name(),
			   f->vec_no, intr_name(f->vec_no));
		intr_dump_frame(f);
		thread_exit();

	case SEL_KCSEG:
		/* Kernel's code segment, which indicates a kernel bug.
		   Kernel code shouldn't throw exceptions.  (Page faults
		   may cause kernel exceptions--but they shouldn't arrive
		   here.)  Panic the kernel to make the point.  */
		intr_dump_frame(f);
		PANIC("Kernel bug - unexpected interrupt in kernel");

	default:
		/* Some other code segment?  Shouldn't happen.  Panic the
		   kernel. */
		printf("Interrupt %#04llx (%s) in unknown segment %04x\n", f->vec_no,
			   intr_name(f->vec_no), f->cs);
		thread_exit();
	}
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.
   (Page fault handler. 이 코드는 가상 메모리를 구현하기 위해 작성된
   스켈레톤 코드 입니다. 프로젝트 2의 일부 솔루션에서도 이 코드를 
   수정해야 할 수 있습니다.)

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". 
   (진입 시, fault가 발생한 주소는 **CR2(Control Register 2)에 저장
   되어 있으며, fault에 대한 정보는 exception.h에 있는 PF_매크로로
   정의된 형식으로 F의 error_code 멤버에 저장되어 있습니다. 여기 있는
   예시 코드는 그 정보를 어떻게 분석할 수 있는지 보여줍니다.
   **Interrupt 14--페이지 폴트 예외(#PF)**에 대한 설명은 [IA32-v3a]문서의
   5.15절 "예외 및 인터럽트 참조"에서 확인할 수 있습니다.)*/
static void page_fault(struct intr_frame *f) {
	bool not_present; /* True: not-present page, false: writing r/o page.  (True: 존재하지 않는 페이지 (not-present page),False: 읽기 전용 페이지에 쓰기 작업을 시도 (writing to read-only page).)*/
	bool write;		  /* True: access was write, false: access was read. True: 쓰기 작업으로 인한 접근 (access was write),False: 읽기 작업으로 인한 접근 (access was read).*/
	bool user;		  /* True: access by user, false: access by kernel. True: 사용자 모드에서의 접근 (access by user),False: 커널 모드에서의 접근 (access by kernel).*/
	void *fault_addr; /* Fault address. */

	/* Obtain faulting address, the virtual address that was
	   accessed to cause the fault.  It may point to code or to
	   data.  It is not necessarily the address of the instruction
	   that caused the fault (that's f->rip). 
	   (폴트를 일으킨 주소, 즉 폴트가 발생한 가상 주소를 가져옵니다. 
	   이 주소는 코드 또는 데이터에 대한 접근일 수 있습니다. 반드시 
	   폴트를 일으킨 명령어의 주소는 아닙니다 (그 주소는 f->rip입니다).)*/

	fault_addr = (void *)rcr2();

	/* Turn interrupts back on (they were only off so that we could
	   be assured of reading CR2 before it changed). 
	   (인터럽트를 다시 활성화합니다 (이 작업은 CR2를 읽는 동안 바뀌지 
	   않도록 하기 위해 인터럽트를 비활성화한 것입니다).)*/
	intr_enable();

	/* Determine cause. (원인을 파악한다.) */
	not_present = (f->error_code & PF_P) == 0;	//not-present page면 0
	write = (f->error_code & PF_W) != 0;	// write면 1 read면 0
	user = (f->error_code & PF_U) != 0;	// kernel이면 0, user면 1

#ifdef VM
	/* For project 3 and later. (프로젝트 3 및 이후 버젼에서.)*/
	if (vm_try_handle_fault(f, fault_addr, user, write, not_present))
		return;
#endif

	/* Count page faults. */
	page_fault_cnt++;
#ifdef USERPROG
	struct process *curr;
	curr = process_current();
	curr->exist_status = -1;
	thread_exit();
#else
	/* If the fault is true fault, show info and exit. 
	(만약 실제 fault라면 정보를 출력하고 종료한다.)*/
	printf("Page fault at %p: %s error %s page in %s context.\n", fault_addr,
		   not_present ? "not present" : "rights violation",
		   write ? "writing" : "reading", user ? "user" : "kernel");
	kill(f);
#endif
}
