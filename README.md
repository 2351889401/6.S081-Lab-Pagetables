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
