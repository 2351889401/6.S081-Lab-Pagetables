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



（1）为每个进程的新页表拷贝（**kernel pagetable**），需要提前考虑内核页表有哪些内容：内核页表初始化时的全部内容 + 所有的内核栈  
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
在“**kernel/vm.c**”中加入函数“**free_copy_kernel_pagetable()**”，并在“**kernel/proc.c**”的“**freeproc()**”函数中调用以释放该新页表  
* 注意，释放该新页表，只需要释放页表相关的页面，而不需要释放任何其他的物理内存，因为：首先内核的代码段和数据段不需要释放；其次，用户进程的物理内存，在进程释放时原来就会释放，这里再释放就会出问题。
```
void free_copy_kernel_pagetable(pagetable_t pagetable, int now_level) {
  int i;
  pte_t pte;
  for(i=0; i<512; i++) {
    pte = pagetable[i];
    if(pte & PTE_V) {
      if(now_level != 3) free_copy_kernel_pagetable((pagetable_t)PTE2PA(pte), now_level+1);
    }
    pagetable[i] = 0;
  }
  kfree((void*)pagetable);
}
```
在在“**kernel/proc.c**”的“**freeproc()**”函数中调用它：
```
// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz); //这里通过调用uvmunmap()函数实现用户页表和内存的释放 所以我们的释放函数不需要释放页表以外的内存
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  // copy_kernel_pagetable里面有的内容: 所有的kernel pagetable里的内容 + 内核栈 + user mappings
  // 需要的方式是将中间所涉及到的所有页表全部清空 然后释放 不去释放真正的物理内存
  free_copy_kernel_pagetable(p->copy_kernel_pagetable, 1);
}
```
  
（2）在新页表中记录下用户页表（**user pagetable**）的全部内容；并且每当用户映射发生改变时，相应的改变应当体现在新页表中（这里主要体现在“**kernel/exec.c/exec()**”，“**kernel/proc.c/fork()**”，“**kernel/proc.c/growproc()**”，“**kernel/proc.c/userinit()**”这些函数中）  
* 注意，实验内容的网页上说，用户虚拟地址空间的最大限制是“**PLIC**”的“**0xC000000**”，而书上的“**CLINT**”的地址是“**0x2000000**”，在实验中我还是限制最大空间不能超过“**CLINT**”，但是仍然通过了全部的测试
所以，这一步骤的核心是在新页表中记录下用户页表（**user pagetable**）的全部内容（与内核页表（**kernel pagetable**）的区别在于内核页表不会动态变化，用户页表可能会变化）
在实现之前，考虑以下4种情况：（**old**表示用户页表，**new**表示新页表，“**无**”表示该页表项为“**0**”，“**有**”表示该页表项不为“**0**”）  
old    new    做法  
无     无     continue  
无     有     （递归的）释放新页表的页表项  
有     有     如果当前是1级或2级查询，递归进入下一级；如果当前是3级查询，让新页表和用户页表指向相同的物理地址（下面解释）  
有     无     如果当前是1级或2级查询，为新页表申请下一级的页表空间，在相同的索引处登记，递归进入下一级；如果当前是3级查询，让新页表和用户页表指向相同的物理地址

这里解释下上面的 “**有     有     如果当前是1级或2级查询，递归进入下一级；如果当前是3级查询，让新页表和用户页表指向相同的物理地址（下面解释）**”的原因：
如果是3级查询，可能存在这样的情况：原来用户页表和新页表指向相同的物理地址，但是释放了那一块内存，并且申请了新的物理内存，让原来的虚拟地址指向新的物理地址。这时，用户页表记录的是对的，而新页表还没有更改，必须和用户页表一致。

```
//目的是让new_pagetable和old_pagetable在 0x2000000 以下的地址存在相同的映射
uint64 LIMIT = CLINT;
void update_pagetable_3(pagetable_t new_pagetable, pagetable_t old_pagetable, int level, uint64 preva) {
  int i;
  for(i=0; i<512; i++) {
    uint64 va = (i << (12 + 9 * (3-level))) | preva; //这一句话很关键，preva是之前等级（1级、2级）查询的虚拟地址，通过“或”运算，让之前等级的虚拟地址和当前的虚拟地址合并
    if(va >= LIMIT) break;
    pte_t new_pte = new_pagetable[i];
    pte_t old_pte = old_pagetable[i];
    if((old_pte & PTE_V) == 0) {
      if((new_pte & PTE_V) == 0) continue; //无 无 的情况
      else { //无 有 的情况
        if(level < 3) free_copy_kernel_pagetable((pagetable_t)PTE2PA(new_pte), level+1);
        new_pagetable[i] = 0;
      }
    }
    else {
      if((new_pte & PTE_V) == 0) { //有 无 的情况
        if(level < 3) {
          char* new_pa = (char*) kalloc();
          memset(new_pa, 0, PGSIZE);
          new_pagetable[i] = PA2PTE(new_pa);
          int perm = PTE_FLAGS(old_pte) & (~PTE_U); // PTE_U权限位必须置0才能在内核中使用
          new_pagetable[i] |= perm;
          update_pagetable_3((pagetable_t)PTE2PA(new_pagetable[i]), (pagetable_t)PTE2PA(old_pte), level+1, va);
        }
        else {
          uint64 new_pa = PTE2PA(old_pte);
          new_pagetable[i] = PA2PTE(new_pa);
          int perm = PTE_FLAGS(old_pte) & (~PTE_U);
          new_pagetable[i] |= perm;
        }
      }
      else{ //有 有 的情况
        if(level < 3) update_pagetable_3((pagetable_t)PTE2PA(new_pte), (pagetable_t)PTE2PA(old_pte), level+1, va);
        else {//这种情况下 相同位置的映射可能不同
          uint64 new_pa = PTE2PA(old_pte);
          new_pagetable[i] = PA2PTE(new_pa);
          int perm = PTE_FLAGS(old_pte) & (~PTE_U);
          new_pagetable[i] |= perm;
        }
      }
    }
  }

}
```
至此，全部完成实验要求，测试结果如下：  
![](https://github.com/2351889401/6.S081-Lab-Pagetables/blob/main/images/result2.png)  
