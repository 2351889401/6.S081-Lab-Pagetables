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
具体的实验内容：xv6内核的用户页表（**user pagetable**）和内核页表（**kernel pagetable**）是分开的。
