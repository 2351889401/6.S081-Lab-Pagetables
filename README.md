## 页表的实验室目前为止做过的最难的一个实验。因为当时对“kernel/vm.c”和“kernel/proc.c”中的函数不熟悉，并且当时对内存管理单元（MMU）、多级页表查询物理地址的流程等理解的不深刻，所以当时做实验实际上跨度大概两周左右，中间好像经历过不少次失败，后来成功完成实验应该是有运气的成分。直到现在，我再看当时写的这份实验的核心代码，仍然感觉做的很不错。  

实验链接（https://pdos.csail.mit.edu/6.S081/2020/labs/pgtbl.html ）  
**1.** 输出一个页表的内容（easy）（比如，查看“**init**”进程在“**exec**”完成时的页表内容）  
在“**kernel/vm.c**”中加入函数“**vmprint()**”  
```
void vmprint(pagetable_t pagetable, int level) {
  if(level == 1) {
    printf("page table %p\n", pagetable);
  }
  for(int i=0; i<512; i++) {
    pte_t pte = pagetable[i];
    if(pte & PTE_V) {
      printf("..");
      for(int j=0; j<level-1; j++) {
        printf(" ..");
      }
      printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));
      if(level < 3) vmprint((pagetable_t)PTE2PA(pte), level+1);
    }
  }
}
```  
“**exec()**”中调用该语句结果为：  
![](https://github.com/2351889401/6.S081-Lab-Pagetables/blob/main/images/result1.png)  
其中，红色方框的内容表示3级页表查询中9位的结果，“**..**”的个数表示了这时第几级页表，将它们组合可以得到完整的虚拟地址；后面的 **pte** 和 **pa** 分别是页表项和真实的物理地址，可以认为它俩差不多，因为 **pa** = （**pte** >> 10）<< 12，**pte**的后10位表示的是权限相关的内容。  

解释图中“**page 0**”和“**page 2**”的内容。在“**user mode**”下能否访问“**page 1**”的内容？  
答：“**page 0**”包含了“**init**”进程的代码段和数据段；“**page 2**”是“**user stack**”，包括了命令行参数、指向命令行参数的指针、返回值地址等。“**page 1**”是“**guard page**”，“PTE_U”被置为0，所以在“**user mode**”下不能访问“**page 1**”的内容。  

## 2. 整体的实验目标：模仿Linux内核，让每个进程的页表包括了 user space 和 kernel space 的内存映射。这样的话，当在用户空间和内核空间切换时，就不用切换页表了。  
具体的实验内容：xv6内核的用户页表（**user pagetable**）和内核页表（**kernel pagetable**）是分开的。当在内核中执行程序时，比如系统调用，此时若需要使用用户空间提供的地址指向的内容（比如，**write()**系统调用传入的用户空间的指针），内核必须首先得到该用户空间提供的地址所对应的物理地址。而内核页表不能解析该用户虚拟地址，所以xv6内核的实现方式是模仿内存管理单元（**MMU**）的3级页表查询流程，通过“**kernel/walkaddr()**”函数（主要是“**kernel/walk()**”）获取该虚拟地址对应的物理地址。  
  
因此，主要的实验做法为：为每个进程创建一个新的域，该域是一个新的页表（我们的实验里面是 “**pagetable_t copy_kernel_pagetable;**”），在该新的页表中记录了内核页表（**kernel pagetable**）的全体和用户页表（**user pagetable**）的全体。有了这样的一个新页表，如果在内核中执行程序时，**MMU**装载了该页表，就可以**直接使用用户空间的地址和内核空间的地址**。因为**MMU**会完成全部的虚拟地址解析，而新页表已经注册了全部所需的虚拟地址。  
主要的流程分为2步：  
（1）在新页表中记录下内核页表（**kernel pagetable**）的全部内容  
（2）在新页表中记录下用户页表（**user pagetable**）的全部内容，并且每当用户映射发生改变时，相应的改变应当体现在新页表中  



（1）为每个进程的新页表拷贝（**kernel pagetable**）  
“**kernel/vm.c**”中加入函数“**copy_kvminit()**”
```
//为每个进程拷贝当前的 kernel pagetbale
void copy_kvminit(pagetable_t pagetable) {
  mappages(pagetable, UART0, PGSIZE, UART0, PTE_R | PTE_W);
  mappages(pagetable, VIRTIO0, PGSIZE, VIRTIO0, PTE_R | PTE_W);
  mappages(pagetable, CLINT, 0x10000, CLINT, PTE_R | PTE_W);
  mappages(pagetable, PLIC, 0x400000, PLIC, PTE_R | PTE_W);
  mappages(pagetable, KERNBASE, (uint64)etext-KERNBASE, KERNBASE, PTE_R | PTE_X);
  mappages(pagetable, (uint64)etext, PHYSTOP-(uint64)etext, (uint64)etext, PTE_R | PTE_W);
  mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X);
}
```
在“**kernel/proc.c**”中的“**allocproc()**”调用“**copy_kvminit()**”函数，这样的话每个进程的新页表和内核页表基本一致了；此外，还需要在“**allocproc()**”中为新页表注册下内核栈的全部地址，因为内核栈也是内核页表的内容  
```
p->copy_kernel_pagetable = (pagetable_t) kalloc(); //分配新页表的空间
memset(p->copy_kernel_pagetable, 0, PGSIZE);

copy_kvminit(p->copy_kernel_pagetable); //与内核页表的内容基本一致 除了内核栈
//做出的结果是让每个进程的 copy_kernel_pagetable 都知道所有的 kernel stack 
struct proc* pp;
for(pp = proc; pp < &proc[NPROC]; pp++) {
    uint64 va = KSTACK((int) (pp - proc));
    uint64 pa = kvmpa(va); //查询内核栈的虚拟地址对应的物理地址 然后去新页表中注册
    mappages(p->copy_kernel_pagetable, va, PGSIZE, pa, PTE_R | PTE_W);
}
```
在“**kernel/proc.c**”中的“**scheduler()**”函数中，如果当前有进程可以执行，**MMU**使用该进程的新页表；另外，如果没有进程执行的时候，使用内核页表（**因为没有进程执行的时候，要注意不能访问内核页表中没有注册的虚拟地址，这样是为了保证隔离性**）
```
if(p->state == RUNNABLE) {
    // Switch to chosen process.  It is the process's job
    // to release its lock and then reacquire it
    // before jumping back to us.
    p->state = RUNNING;
    c->proc = p;

    //当前进程要执行的时候 切换为它的新页表 这样就可以直接使用用户空间的虚拟地址了 在（1）中还没达到这个效果 但是完成了（2）就可以达到了
    w_satp(MAKE_SATP(p->copy_kernel_pagetable));
    sfence_vma();
    
    swtch(&c->context, &p->context);
    
    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
    
    found = 1;
    
    //当前进程执行完毕后 应当恢复kernel_pagetable
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();
}
```
