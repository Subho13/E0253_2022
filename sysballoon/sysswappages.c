#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include <linux/fs.h>
#include <asm/pgtable.h>

// unfruitful attempts at freeing page:
// #include <linux/swap.h> // for nr_free_pages()
// #include <asm/pgalloc.h> // for pte_free()
// #include <asm/tlbflush.h> // for flush_tlb_page()
// #include <linux/pagemap.h> // for release_pages()
// #include <linux/mm.h> // for put_page()
#include <linux/rmap.h> // for try_to_unmap() and ttu_flags

#define SWAP_PAGES_COUNT 128 * 1024
pte_t *swappedOutPtes[SWAP_PAGES_COUNT];


extern void free_unref_page(struct page *page);
extern struct task_struct *processRegisteredForBallooning;

extern int my_int_len(int n);
extern void my_itoa(int n, char *buff);

void swap_my_pages(void *buff, int pages, pte_t *pte) {
    char swapFileName[64] = "/ballooning/swap_\0";
    char pid[8];
    my_itoa(processRegisteredForBallooning->pid, pid);
    strcat(swapFileName, pid);

    // int mode = 0666;
	int flags = O_RDWR;
    struct file* swapFile;
    int i;
    loff_t offset;
    swapFile = filp_open(swapFileName, flags, 0);

    for (i = 0; i < SWAP_PAGES_COUNT; i++) {
        if (swappedOutPtes[i] == NULL) break;
    }
    offset = i * PAGE_SIZE;
    swappedOutPtes[i] = pte;
    for (i = 0; i < pages; i++) {
        kernel_write(swapFile, buff, PAGE_SIZE, &offset);
        if (i >= SWAP_PAGES_COUNT) break;
    }

    filp_close(swapFile, processRegisteredForBallooning->files);
}

int try_to_swap_out(void *address, int pages) {
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep, pte;
    struct page *page;
    unsigned long freeMemoryKB1, freeMemoryKB2;
    struct mm_struct *mm = current->mm;

    pgd = pgd_offset(mm, (unsigned long)address);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return -1;
    
    p4d = p4d_offset(pgd, (unsigned long)address);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
        return -1;

    pud = pud_offset(p4d, (unsigned long)address);
    if(pud_none(*pud) || pud_bad(*pud))
        return -1;

    pmd = pmd_offset(pud, (unsigned long)address);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
        return -1;
  
    ptep = pte_offset_map(pmd, (unsigned long)address);
    pte = *ptep;

    if (!pte_present(pte))
        return -1;

    page = pte_page(pte);
    swap_my_pages(address, pages, ptep);

    // Reference: https://stackoverflow.com/questions/10265188/how-to-get-to-struct-vm-area-struct-from-struct-page
    struct anon_vma *anonvma;
    struct anon_vma_chain *vmac;
    struct vm_area_struct *vma;
    anonvma = page_get_anon_vma(page);
    anon_vma_lock_read(anonvma);
    anon_vma_interval_tree_foreach(vmac, &anonvma->rb_root, 0, ULONG_MAX) {
        vma = vmac->vma;
        if (page_address_in_vma(page, vma)) goto found_vma_of_page;
    }
found_vma_of_page:
    anon_vma_unlock_read(anonvma);

    zap_page_range(vma, page_address_in_vma(page, vma), pages * PAGE_SIZE);

    return 0;
}

SYSCALL_DEFINE2(swap_pages, void *, buff, int, pages)
{
	printk("Received system call [swap_pages] from process (pid: %d)\n", current->pid);

    try_to_swap_out(buff, pages);
    return 0;
}
