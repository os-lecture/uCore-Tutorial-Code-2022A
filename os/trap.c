#include "trap.h"
#include "defs.h"
#include "loader.h"
#include "syscall.h"
#include "timer.h"

extern char trampoline[], uservec[];
extern char userret[];

void kerneltrap()
{
	if ((r_sstatus() & SSTATUS_SPP) == 0)
		panic("kerneltrap: not from supervisor mode");
	panic("trap from kerne");
}

// set up to take exceptions and traps while in the kernel.
void set_usertrap(void)
{
	w_stvec(((uint64)TRAMPOLINE + (uservec - trampoline)) & ~0x3); // DIRECT
}

void set_kerneltrap(void)
{
	w_stvec((uint64)kerneltrap & ~0x3); // DIRECT
}

// set up to take exceptions and traps while in the kernel.
void trap_init(void)
{
	// intr_on();
	set_kerneltrap();
}

void unknown_trap()
{
	errorf("unknown trap: %p, stval = %p", r_scause(), r_stval());
	exit(-1);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap()
{
	set_kerneltrap();
	struct trapframe *trapframe = curr_proc()->trapframe;
	tracef("trap from user epc = %p", trapframe->epc);
	if ((r_sstatus() & SSTATUS_SPP) != 0)
		panic("usertrap: not from user mode");

	uint64 cause = r_scause();
	if (cause & (1ULL << 63)) {
		cause &= ~(1ULL << 63);
		switch (cause) {
		case SupervisorTimer:
			tracef("time interrupt!");
			set_next_timer();
			yield();
			break;
		default:
			unknown_trap();
			break;
		}
	} else {
		switch (cause) {
		case UserEnvCall:
			trapframe->epc += 4;
			syscall();
			break;
		case StorePageFault:
		case LoadPageFault:
			{
				uint64 addr = (uint64)r_stval();
				if(lazy_alloc(addr) < 0){
					errorf("lazy_aolloc() < 0");
				}
			//struct proc *p = curr_proc();	
			/*if(uvmshouldtouch(va)){
				errorf("Page Fault %d in application, bad addr = %p, bad instruction = %p, "
                               "core dumped.", cause, r_stval(), trapframe->epc);
				uvmlazytouch(va);
			}
			else{
				errorf("Unexpexted Scause %d in application, bad addr = %p, bad instruction = %p, "
                               "core dumped.",cause, r_stval(), trapframe->epc);
				//p->killed = 1;
			}*/	
			//exit(-2);
				break;
	       		}		
		case StoreMisaligned:
		case InstructionMisaligned:
		case InstructionPageFault:
		case LoadMisaligned:
			errorf("%d in application, bad addr = %p, bad instruction = %p, "
			       "core dumped.",
			       cause, r_stval(), trapframe->epc);
			exit(-2);
			break;
		case IllegalInstruction:
			errorf("IllegalInstruction in application, core dumped.");
			exit(-3);
			break;
		default:
			unknown_trap();
			break;
		}
	}
	usertrapret();
}

//
// return to user space
//
void usertrapret()
{
	set_usertrap();
	struct trapframe *trapframe = curr_proc()->trapframe;
	trapframe->kernel_satp = r_satp(); // kernel page table
	trapframe->kernel_sp =
		curr_proc()->kstack + KSTACK_SIZE; // process's kernel stack
	trapframe->kernel_trap = (uint64)usertrap;
	trapframe->kernel_hartid = r_tp(); // unuesd

	w_sepc(trapframe->epc);
	// set up the registers that trampoline.S's sret will use
	// to get to user space.

	// set S Previous Privilege mode to User.
	uint64 x = r_sstatus();
	x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
	x |= SSTATUS_SPIE; // enable interrupts in user mode
	w_sstatus(x);

	// tell trampoline.S the user page table to switch to.
	uint64 satp = MAKE_SATP(curr_proc()->pagetable);
	uint64 fn = TRAMPOLINE + (userret - trampoline);
	tracef("return to user @ %p", trapframe->epc);
	((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}


int lazy_alloc(uint64 addr){
struct proc *p = curr_proc();
  // page-faults on a virtual memory address higher than any allocated with sbrk()
  // this should be >= not > !!!
  if (addr >= p->program_brk) {
    errorf("lazy_alloc: access invalid address");
    return -1;
  }

  if (addr < p->heap_bottom) {
    errorf("lazy_alloc: access address below stack");
    return -2;
  }
  
  uint64 pa = PGROUNDDOWN(addr);
  char* mem = kalloc();
  if (mem == 0) {
    errorf("lazy_alloc: kalloc failed");
    return -3;
  }
  
  memset(mem, 0, PGSIZE);
  if(mappages(p->pagetable, pa, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
    kfree(mem);
    return -4;
  }
  return 0;
}

// touch a lazy-allocated page so it's mapped to an actual physical page.
/*void uvmlazytouch(uint64 va) {
	struct proc *p = curr_proc();
	char *mem = kalloc();
	if(mem == 0) {
    		// failed to allocate physical memory
   		errorf("lazy alloc: out of memory\n");
   	 	//p->killed = 1;
  	} else {
    		memset(mem, 0, PGSIZE);
    		if(mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      			errorf("lazy alloc: failed to map page\n");
      			kfree(mem);
      			//p->killed = 1;
    		}	
  	}
  	// printf("lazy alloc: %p, p->sz: %p\n", PGROUNDDOWN(va), p->sz);
}

// whether a page is previously lazy-allocated and needed to be touched before use.
int uvmshouldtouch(uint64 va) {
  	pte_t *pte;
  	struct proc *p = curr_proc();

  	return va < p->program_brk // within size of memory for the process
        && PGROUNDDOWN(va) != p->heap_bottom // not accessing stack guard page (it shouldn't be mapped)
    	&& (((pte = walk(p->pagetable, va, 0))==0) || ((*pte & PTE_V)==0)); // page table entry does not exist
}*/
