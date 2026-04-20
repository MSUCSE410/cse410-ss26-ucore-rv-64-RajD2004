
#include "loader.h"
#include "defs.h"
#include "trap.h"


static uint64 app_num;
static uint64 *app_info_ptr;
extern char _app_num[], ekernel[];
extern char trampoline[];


// Count finished programs. If all apps exited, shutdown.
int finished()
{
	static int fin = 0;
	if (++fin >= app_num)
		panic("all apps over");
	return 0;
}

// Get user progs' infomation through pre-defined symbol in `link_app.S`
void loader_init()
{
	// if ((uint64)ekernel >= BASE_ADDRESS) {
	// 	panic("kernel too large...\n");
	// }
	app_info_ptr = (uint64 *)_app_num;
	app_num = *app_info_ptr;
	app_info_ptr++;
}

// Load nth user app at
// [BASE_ADDRESS + n * MAX_APP_SIZE, BASE_ADDRESS + (n+1) * MAX_APP_SIZE)
int load_app(int n, uint64 *info)
{
	uint64 start = info[n], end = info[n + 1], length = end - start;
	memset((void *)BASE_ADDRESS + n * MAX_APP_SIZE, 0, MAX_APP_SIZE);
	memmove((void *)BASE_ADDRESS + n * MAX_APP_SIZE, (void *)start, length);
	return length;
}

// load all apps and init the corresponding `proc` structure.
int run_all_app()
{
	for (int i = 0; i < app_num; ++i) {
		struct proc *p = allocproc();
		uint64 start = app_info_ptr[i];
		uint64 end = app_info_ptr[i + 1];
		uint64 length = end - start;

		pagetable_t pg = uvmcreate();
		mappages(pg, TRAPFRAME, PGSIZE, (uint64)p->trapframe, PTE_R | PTE_W);

		uint64 num_pages = PGROUNDUP(length) / PGSIZE;
		for (uint64 j = 0; j < num_pages; j++) {
			void *pa = kalloc();
			memset(pa, 0, PGSIZE);
			uint64 off = j * PGSIZE;
			uint64 cplen = length - off > PGSIZE ? PGSIZE : length - off;
			memmove(pa, (void *)(start + off), cplen);
			mappages(pg, BASE_ADDRESS + off, PGSIZE, (uint64)pa, PTE_U | PTE_R | PTE_W | PTE_X);
		}

		void *ustack_pa = kalloc();
		memset(ustack_pa, 0, PGSIZE);
		uint64 ustack_bottom = BASE_ADDRESS + num_pages * PGSIZE + PGSIZE;
		mappages(pg, ustack_bottom, PGSIZE, (uint64)ustack_pa, PTE_U | PTE_R | PTE_W);

		p->pagetable = pg;
		p->ustack = ustack_bottom;
		p->max_page = PGROUNDUP(ustack_bottom + USTACK_SIZE - 1) / PGSIZE;
		p->trapframe->epc = BASE_ADDRESS;
		p->trapframe->sp = ustack_bottom + USTACK_SIZE;
		p->state = RUNNABLE;
		p->start_time = 0;
		for (int j = 0; j < 500; j++) p->syscall_times[j] = 0;
	}
	return 0;
}
