#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// extern pagetable_t kernel_pagetable;
// extern pte_t *
// walk(pagetable_t pagetable, uint64 va, int alloc);

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
#if defined(LAB_PGTBL) || defined(LAB_LOCK)
    statsinit();
#endif
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    // uint32 x = 0xffffffff;
    // printf("x: %d\n", x);
    // printf("x+10: %d\n", x+10);
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
#ifdef LAB_NET
    pci_init();
    sockinit();
#endif    
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  //查看kernel启动后地址0x2000000是否合法
  // uint64 va1 = 0x1000000;
  // pte_t *pte;
  // pte = walk(kernel_pagetable, va1, 0);
  // printf("CLINT's pa address is %p\n", kvmpa(va1));
  // printf("pte = %p\n", *pte);
  // printf("%p\n", PTE2PA(*pte));

  scheduler();        
}
