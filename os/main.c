#include "console.h"
#include "defs.h"
#include "loader.h"
#include "timer.h"
#include "trap.h"

void clean_bss()
{
	extern char s_bss[];
	extern char e_bss[];
	memset(s_bss, 0, e_bss - s_bss);
}

void main()
{
	clean_bss();
	printf("hello world!\n");
	proc_init();
	printf("proc_init finished\n");
	kinit();
	printf("kinit finished\n");
	kvm_init();
	printf("kvm_init finished\n");
	loader_init();
	printf("loader_init finished\n");
	trap_init();
	printf("trap_init finished\n");
	timer_init();
	printf("timer_init finished\n");
	run_all_app();
	infof("start scheduler!");
	scheduler();
}
