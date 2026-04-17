#ifndef CPU_PROC_LIST_H
#define CPU_PROC_LIST_H

task_t* create_task(void (*entry_point)(), uint32_t pdir, const char* name);

#endif
