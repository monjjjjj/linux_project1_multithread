> 第9組 
> 111522030 李孟潔
> 111522061 鄭伊涵
> 111526003 張友安

## Enviroment
* VMware
* Ubuntu 16.04 Lts
* Linux Kernel 4.15.1

## Kernel 編譯過程
### 遇到的問題
**1. memory空間不足**
>解決方法: 重割VM
   
**2. 使用oldconfig會出現error**
```c=
No rule to make target 'debian/canonical-certs.pem', needed by 'certs/x509_certificate_list'
```
>修改CONFIG_SYSTEM_TRUSTED_KEYS，將其值改為空值
   
>修改CONFIG_SYSTEM_REVOCATION_KEYS，將其值改為空值


```c=
BTF: .tmp_vmlinux.btf: pahole (pahole) is not available
```
>sudo apt install dwarves

修改完後編譯仍無法順利完成，後來改用menuconfig
   
**3. Kernel code版本相容性問題**
>一開始使用uname -r確認kernel版本為: 4.15.0-142-generic
>認為Kernel source code用4.15即可，但編譯過後仍有問題，最後使用4.15.1的版本才可以順利編譯過!
>向助教確認過，任何kernel版本都能向上支援!



## Kernel Space
### Process
* **Virtual address 轉換成 Physical address** 
![](https://i.imgur.com/FGtABhz.png)

>64bits的linux kernel架構使用了4層的page table來做位址的轉換，分別為:
>-- Page Global Directory (PGD)
>-- Page Upper Directory (PUD)
>-- Page Middle Directory (PMD)
>-- Page Table (PTE)

>每一級table有3個key description marco: shift, size, mask。

>PGD, PUD, PMD, PTE會分別由pgd_t, pud_t, pmd_t, pte_t來做描述。

### SYSCALL_DEFINE
```c=
SYSCALL_DEFINE3(get_physical_address, unsigned long*, initial, unsigned long*, result, int, n)
```
#### Overview of the CVE-2009-0029
> The ABI in the Linux kernel 2.6.28 and earlier on s390, powerpc, sparc64, 
> and mips 64-bit platforms requires that a 32-bit argument in a 64-bit 
> register was properly sign extended when sent from a user-mode application, 
> but cannot verify this, which allows local users to cause a denial of 
> service (crash) or possibly gain privileges via a crafted system call.

> 在lINUX2.6.28以及之前版本的KERNEL中，架構為64位元的平台的ABI再要求System Call時，在
> User Space的Process若想將32位元的參數存放在64位元的暫存器時，要做到正確的sign 
> extension，但是在User Space的Process是無法保證能做到這一點，因此就有可能透過傳送參數
> 來導致系統崩潰或者以非常理的方式拿到更高的權限。

> 比如說一個函數具有32位元的int parameter，惡意的使用者可能會將一個值放入暫存器，但該值可能會大於int可能存在的值，且Compiler不易檢查到該值是否超出int的範圍。
> 為了解決上述問題，SYSCALL_DEFINE會先假定所有參數都是64bits的long value，然後再將它們
> 轉換回實際的類型!

> 在x86_64和Itanium，處理器當拿到一個32位元的值時，會忽略掉暫存器中的upper bits。

### copy_from_user & copy_to_user
```c=
ret = copy_from_user(va, initial, sizeof(*va)*n);
ret = copy_to_user(result, pa, sizeof(*pa)*n);
```
#### Overview
> user-space 無法「直接」存取 kernel-space 的記憶體，因此不能使用memcpy!

> 在syscall中，由於user space與kernel space算是不同的process且權限也不一樣，兩者之間的
> 記憶體是無法共用的，因此在傳遞變數時只傳值的話是無法改變變數的數值，所以才需要透過kernel的API: copy_to_user()和copy_from_user() 來傳遞參數。
> 前者是在kernel把位址傳回user space，後者則是將位址傳遞到kernel space。

#### copy_to_user
> to：資料的目的位址，此參數為一個指向 user-space 記憶體的指標。
> from：資料的來源位址，此參數為一個指向 kernel-space 記憶體的指標。
> **copy data to user-space from kernel-space**

#### copy_from_user
>  to：資料的目的位址，此參數為一個指向 kernel-space 記憶體的指標。
>  from：資料的來源位址，此參數為一個指向 user-space 記憶體的指標。
>  **copy data from user-space to kernel-space**
### Current 指標

```c=
#ifndef __ASM_GENERIC_CURRENT_H
#define __ASM_GENERIC_CURRENT_H

#include <linux/thread_info.h>

#define get_current() (current_thread_info()->task)
#define current get_current()

#endif /* __ASM_GENERIC_CURRENT_H */
```
>可以看出current實際上就是get_current()函數。
>get_current()又調用了current_thread_info()。
>以下是current_thread_info()的prototype
```c=
static inline struct thread_info *current_thread_info(void)
{
	register unsigned long sp asm ("sp");
	return (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
}
```
>當kernel thread執行到此處時，其SP的pointer指向調用process所对应的kernel thread的stack頂部。通過sp & ~(THREAD_SIZE-1)向上對齊，達到stack底部。
>將結果轉會為thread_info類型，此type中有一個成員为task_struct，它就是目前正在運行process的task_struct pointer。
* **程式碼解釋**
```c=
SYSCALL_DEFINE3(get_physical_addresses, unsigned long*, initial, unsigned long*, result, int , n)  
```
> initial = virtual address
> result = physical address
> int = # of VA and PA elements
> 

```c=
va = (unsigned long*)kmalloc(sizeof(unsigned long)*n,GFP_KERNEL);
pa = (unsigned long*)kmalloc(sizeof(unsigned long)*n,GFP_KERNEL); 
```
>-- kmalloc is the normal method of allocating memory for objects smaller than page size in the kernel.
>-- GFP_KERNEL: Allocate normal kernel ram.


```c=
copy_from_user(va, initial, sizeof(*va)*n);
```
>從user space code拷貝user space中的virtual address到kernel space中宣告的va


```c=
for (i = 0; i < n; i++)
{
        pgd = pgd_offset(current->mm, va[i]);
        pud = pud_offset((p4d_t*)pgd, va[i]);
        pmd = pmd_offset(pud, va[i]);
        pte = pte_offset_kernel(pmd, va[i]);

        page_addr = pte_val(*pte) & PAGE_MASK;
        page_offset = va[i] & ~PAGE_MASK;
        pa[i] = page_addr | page_offset;
}
```
>流程架構模擬如下圖

![](https://i.imgur.com/i3RsTDf.png)

>-- pgd_offset: 根據目前的virtual address和目前process的mm_struct，可得到pgd entry 
    (entry內容為pud table的 base address)
>-- pud_offset: 根據透過pgd_offset得到的pgd entry和virtual address，可得到pud entry
    (entry內容為pmd table的base address)
>-- pmd_offset: 根據pud entry的內容和virtual address，可得到pte table的base address
>-- pte_offset: 根據pmd entry的內容與virtual address，可得到pte的base address
>-- 將從pte得到的base address與Mask(0xf...fff000)做and運算，及可得到page的base physical address
>-- virtual address與~Mask(0x0...000fff)做and運算得到offset，再與page的base physical address做or運算即可得到轉換過後的完整的physical address。
>-- 將所得的physical address放入pa，繼續進行前面的動作，直到迴圈結束。

>以下為pgd_offset()相關macro
```c=
#define pgd_offset(mm, address) pgd_offset_pgd((mm)->pgd, (address))
#define pgd_offset_pgd(pgd, address) (pgd + pgd_index((address)))
//base addr + offset
#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
//(1000000000 - 1 = 0111111111) 取後9個bits
#define PGDIR_SHIFT	39
#define PTRS_PER_PGD	512  //pgd的entry個數
```
>與上方架構圖對比，可以看出pgd_offset找出下一層pud的base address是用macro所寫的去找出entry。
>virtual address除了前面16-bit是unused之外，後面是照者9、9、9、9、12 bits去分割的(共48個bits)，從PGDIR_SHIFT可以看出來。
```c=
copy_to_user(result, pa, sizeof(*va)*n);
```
>將轉換完成的physical address從kernel space拷貝到user space中result所在的位置，這樣在user code中就可以取得我們所要的physical address。

```c=
kfree(va);
kfree(pa);
```
>最後記得要free掉當初kmalloc的記憶體，以免造成程式crash掉。


## User Space
### Process
* **設計流程**
    >1. 使用 pthread_create 建立 3 個 thread
    >2. 使用 pthread_join 等待各 thread 結束
    >3. 使用 pthread_mutex_t 避免 thread 之間衝突
    >4. 使用 size_t* 型態存放 virtual address 和 physical address
    >5. 使用自定義 syscall 取得 va 中之實體位址並放入 pa
    >6. 印出各項 virtual address 和 physical address 作為結論分析依據
    >7. 利用 threads 陣列取得各 thread 的 size, start address, end address
* **在 global scope 觀察 Data 和 BSS 的記憶體位址**
    >1. 定義 init_data 以觀察 Data 的記憶體位址
    >2. 宣告 non_init_data 以觀察 BSS 的記憶體位址**
* **在 main 中觀察 stack, code, Thread-Local Storage, library 的記憶體位址**
    >1. 定義 local_data 以觀察 stack 的記憶體位址
    >2. 利用 dlopen 以 RTLD_LAZY 的方式取得動態連接庫位址，並藉由 dlsym 取得 printf 的記憶體位址
    >3. 用 &our_func 觀察 code 的記憶體位址
    >4. 觀察 tls 的記憶體位址是否改變
    >5. 定義 heapAddress，藉由 malloc 分配記憶體以觀察 heap 的記憶體位址
    >6. 宣告 threads 陣列，觀察在 process 的 stack 中 threads[i] 的大小、起始記憶體位址及結束的記憶體位址
* **在 our_func 觀察 stack, code, Thread-Local Storage, library 的記憶體位址**
    >1. 觀察 main 1~5 步驟中所提及之記憶體位址
* **pthread 和 fork 的差別**
    >1. pthread 為建立 thread 的函式，fork 為建立 process 的函式，而其在 linux 底層下都是執行 copy_process 去創建 process，差別只是在傳入的參數不同，以決定是否要複製(共用)當前 process 下的一些資訊
* **main thread 和 create_thread 中， mm_struct 和 task_struct 的差別**
    >1. 每個 process 都只會有一個 task_struct，用以存儲該 process 的各種資訊，如mm_struct, fs_struct, files_struct 等
    >2. mm_struct 記錄了該 process 相關的位址空間如 stack, MMAP, Heap, BBS等
    >3. 在 mm_struct 中，會有一個存放一個 stack 起始位置提供給 stack 存放空間。而如果是一個 Thread，會在 heap 或 mmap 內找一個空間作為 Thread 的 stack。
    >4. 無論是 fork 或是 pthread_create，task_struct 都會實例化，而在 thread 中 mm_struct 會與 main thread 共用

## 執行畫面
![](https://i.imgur.com/EbOVT3l.jpg)
![](https://i.imgur.com/JUiHsas.jpg)
- 可看出main thread與2個子thread的data, bss, library, code segment是為在相同的physical address起始位置上，而stack, thread local storage, heap segment的位址是不一樣的!
- thread local storage為每個thread獨立使用的空間，因此每個tls的位址會不一樣!
- 不管是main thread, thread1 or thread2，他們的PGID都是 18451。
- 每個thread的PID為不一樣的!
  >main thread = 0x7f8ffaaca700
  >thread1     = 0x7f8ffa0d2700
  >thread2     = 0x7f8ff98d1700
  >

## Result
![](https://i.imgur.com/Z5Scxij.jpg)

## Reference
-課堂提供的文件

https://view.officeapps.live.com/op/view.aspx?src=https%3A%2F%2Fstaff.csie.ncu.edu.tw%2Fhsufh%2FCOURSES%2FFALL2022%2Flinux_3.10_manual_2022.pptx&wdOrigin=BROWSELINK

https://view.officeapps.live.com/op/view.aspx?src=https%3A%2F%2Fstaff.csie.ncu.edu.tw%2Fhsufh%2FCOURSES%2FFALL2016%2Freminder.docx&wdOrigin=BROWSELINK

-Kernel code & System call

https://blog.kaibro.tw/2016/11/07/Linux-Kernel%E7%B7%A8%E8%AD%AF-Ubuntu/

https://dev.to/jasper/adding-a-system-call-to-the-linux-kernel-5-8-1-in-ubuntu-20-04-lts-2ga8

-CONFIG

https://blog.csdn.net/qq_36393978/article/details/118157426

https://stackoverflow.com/questions/61657707/btf-tmp-vmlinux-btf-pahole-pahole-is-not-available

-PGD, PMD, PTE

https://www.cnblogs.com/sky-heaven/p/5660210.html

-Multi-thread

https://haogroot.com/2020/12/20/pthread-note/#:~:text=%E9%80%9A%E5%B8%B8%E7%94%B1%20main%20thread%20%E4%BE%86%E5%88%86%E9%85%8D%E8%A8%98%E6%86%B6%E9%AB%94%E8%88%87%E9%87%8B%E6%94%BE%E8%A8%98%E6%86%B6%E9%AB%94%E6%9C%83%E6%98%AF%E6%AF%94%E8%BC%83%E5%A5%BD%E7%9A%84%E4%BD%9C%E6%B3%95%EF%BC%8C%E5%A6%82%E4%B8%8B%E9%9D%A2%E7%AF%84%E4%BE%8B%EF%BC%8C%E5%B0%87%E8%B3%87%E6%96%99%E5%A1%9E%E9%80%B2%E4%B8%80%E5%80%8B,struct%20%E8%A3%A1%EF%BC%8C%E7%B5%B1%E4%B8%80%E7%94%B1%20main%20thread%20%E7%AE%A1%E7%90%86%E8%A8%98%E6%86%B6%E9%AB%94%E3%80%82

https://ithelp.ithome.com.tw/articles/10280830

https://medium.com/@chinhung_liu/work-note-pthread-e908d53b557e

-ThreadID

https://stackoverflow.com/questions/21091000/how-to-get-thread-id-of-a-pthread-in-linux-c-program#:~:text=Linux%20provides%20such%20system%20call%20to%20allow%20you,gives%20you%20pid%20%2C%20each%20threadid%20and%20spid.

-位址轉換圖片來源

https://www.chegg.com/homework-help/questions-and-answers/given-virtual-memory-areas-used-process-write-system-call-sysgetpageinfo-report-current-st-q46483922

https://0xax.gitbooks.io/linux-insides/content/Theory/linux-theory-1.html

-SYSCALL_DEFINE

https://nvd.nist.gov/vuln/detail/CVE-2009-0029

https://blog.csdn.net/hxmhyp/article/details/22699669

https://blog.csdn.net/hxmhyp/article/details/22619729

https://www.quora.com/What-is-Kernel-ABI

https://stackoverflow.com/questions/18885045/confusion-about-cve-2009-0029-linux-kernel-insecure-64-bit-system-call-argument

-current 指標

https://blog.csdn.net/u010154760/article/details/45676723








