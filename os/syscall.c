#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

//change sys_write
uint64 sys_write(int fd, uint64 va, uint len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	if (fd != STDOUT)
		return -1;
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

//change sys_gettimeofday 
uint64 sys_gettimeofday(uint64 val_va, int _tz)
{
	struct proc *p = curr_proc();
	TimeVal *val = (TimeVal *)useraddr(p->pagetable, val_va);
	if (val == 0)
		return -1;
	uint64 cycle = get_cycle();
	val->sec = cycle / CPU_FREQ;
	val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

/*
* LAB1: you may need to define sys_task_info here
*/
///change sys_task_info
int sys_task_info(uint64 ti_va)
{
	struct proc *p = curr_proc();
	TaskInfo *ti = (TaskInfo *)useraddr(p->pagetable, ti_va);
	if (ti == 0)
		return -1;
	ti->status = Running;
	for (int i = 0; i < 500; i++) {
		ti->syscall_times[i] = p->syscall_times[i];
	}
	ti->time = (get_cycle() - p->start_time) / (CPU_FREQ / 1000);
	return 0;
}

///add the 2 new functions
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd)
{
	if (!PGALIGNED(start))
		return -1;
	if (len == 0)
		return 0;
	if (len > 1024UL * 1024 * 1024)
		return -1;
	if (port & ~0x7)
		return -1;
	if ((port & 0x7) == 0)
		return -1;

	struct proc *p = curr_proc();
	uint64 end = PGROUNDUP(start + len);
	int perm = PTE_U | ((port & 0x7) << 1);

	for (uint64 va = start; va < end; va += PGSIZE) {
		if (walkaddr(p->pagetable, va) != 0)
			return -1;
	}

	for (uint64 va = start; va < end; va += PGSIZE) {
		void *pa = kalloc();
		if (pa == 0)
			return -1;
		memset(pa, 0, PGSIZE);
		if (mappages(p->pagetable, va, PGSIZE, (uint64)pa, perm) != 0) {
			kfree(pa);
			return -1;
		}
	}

	uint64 new_max = end / PGSIZE;
	if (new_max > p->max_page)
		p->max_page = new_max;
	
	return 0;
}

uint64 sys_munmap(uint64 start, uint64 len)
{
	if (!PGALIGNED(start))
		return -1;
	if (len == 0)
		return 0;

	struct proc *p = curr_proc();
	uint64 end = PGROUNDUP(start + len);

	for (uint64 va = start; va < end; va += PGSIZE) {
		if (walkaddr(p->pagetable, va) == 0)
			return -1;
	}

	uvmunmap(p->pagetable, start, (end - start) / PGSIZE, 1);
	return 0;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	curr_proc()->syscall_times[id]++;

	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);   ///this line changed
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);  ///this line changed
		break;
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/

	case SYS_taskinfo:
		ret = sys_task_info(args[0]);  ///this line changed
		break;

	///add 2 new cases 
	case SYS_mmap:
		ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap(args[0], args[1]);
		break;

	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}