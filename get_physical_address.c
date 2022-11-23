#include <linux/syscalls.h>
#include <linux/printk.h>
#include <linux/mm_types.h>
#include <linux/slab.h>

SYSCALL_DEFINE3(get_physical_address, unsigned long*, initial, unsigned long*, result, int, n)
{
	pgd_t* pgd;
	pud_t* pud;
	pmd_t* pmd;
	pte_t* pte;

	unsigned long page_addr = 0;
	unsigned long page_offset = 0;
	unsigned long* va;
	unsigned long* pa;
	int i;
	long ret = 0;
	va = (unsigned long*)kmalloc(sizeof(unsigned long)*n,GFP_KERNEL);
	pa = (unsigned long*)kmalloc(sizeof(unsigned long)*n,GFP_KERNEL);
	ret = copy_from_user(va, initial, sizeof(*va)*n);
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
	ret = copy_to_user(result, pa, sizeof(*pa)*n);
	kfree(va);
	kfree(pa);
	return ret;

}
