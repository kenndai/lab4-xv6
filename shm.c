#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct shm_page {
    uint id; // memory segment id
    char *frame; // pointer to the physical page we share
    int refcnt; // number of processes sharing this page
  } shm_pages[64];
} shm_table;

void shminit() {
  int i;
  initlock(&(shm_table.lock), "SHM lock");
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {
    shm_table.shm_pages[i].id =0;
    shm_table.shm_pages[i].frame =0;
    shm_table.shm_pages[i].refcnt =0;
  }
  release(&(shm_table.lock));
}

// Lab4
// loop through shm_table's shm_pages to see if the segment id exists already
// If id exists already increase reference cnt, refcnt
// call mappages to add the mapping between the virtual address and the physical address
// in either case return the virtual address through the second parameter of the system call.
int shm_open(int id, char **pointer) {
    struct proc* curproc = myproc();
    int idExists = 0;
    uint va = PGROUNDUP(curproc->sz);
    char* newPA; // address returned by kalloc()
    int i;

    acquire(&(shm_table.lock));
    for (i = 0; i < 64; i++) {
        if (shm_table.shm_pages[i].id == id) {
            idExists = 1;
            mappages(curproc->pgdir, (void*) va, PGSIZE, V2P(shm_table.shm_pages[i].frame), PTE_W | PTE_U);
            shm_table.shm_pages[i].refcnt++;
            break;
        }
    }

    // first to do shm_open()
    if (!idExists) {
        i = 0;
        while (shm_table.shm_pages[i].id != 0)
            i++;
        shm_table.shm_pages[i].id = id;

        // custom allocuvm()
        if ((newPA = kalloc()) == 0) {
            cprintf("custom allocuvm out of memory!\n");
            return 1;
        }
        memset(newPA, 0, PGSIZE);
        mappages(curproc->pgdir, (void*) va, PGSIZE, V2P(newPA), PTE_W | PTE_U);

        shm_table.shm_pages[i].frame = newPA;
        shm_table.shm_pages[i].refcnt = 1;
    }

    curproc->sz = va + PGSIZE;
    *pointer = (char*)va;
    release(&(shm_table.lock));
    return 0; //added to remove compiler warning -- you should decide what to return
}

// Lab4
// Loop through shm_table, if segment id exists, decrement the reference count.
// If reference count reaches zero clear the shm_table. You do not need to free up the page since it is still mapped in the page table.
// Okay to leave it that way.
int shm_close(int id) {
    int clearTable = 0;
    int i;

    acquire(&(shm_table.lock));
    for (i = 0; i < 64; i++) {
        if (shm_table.shm_pages[i].id == id) {
            if ((shm_table.shm_pages[i].refcnt--) == 0) {
                // need to clear shm_table
                clearTable = 1;
                break;
            }
        }
    }

    // if refcnt == 0
    if (clearTable) {
        for (i = 0; i < 64; i++) {
            shm_table.shm_pages[i].id = 0;
            shm_table.shm_pages[i].frame = 0;
            shm_table.shm_pages[i].refcnt = 0;
        }
    }

    release(&(shm_table.lock));
    return 0; //added to remove compiler warning -- you should decide what to return
}
