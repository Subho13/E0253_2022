#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include <linux/fs.h>
#include <asm/pgtable.h>

// unfruitful attempts at freeing page:
#include <linux/swap.h> // for nr_free_pages()
// #include <asm/pgalloc.h> // for pte_free()
// #include <asm/tlbflush.h> // for flush_tlb_page()
// #include <linux/pagemap.h> // for release_pages()
// #include <linux/mm.h> // for put_page()
#include <linux/rmap.h> // for try_to_unmap() and ttu_flags
#include <linux/mman.h> // for MADV_PAGEOUT

#include "sigballoon.h"
unsigned long swappedOutPtes[SWAP_PAGES_COUNT];
#define FREE_MEMORY_LIMIT (1*1024*1024)

extern void free_unref_page(struct page *page);
extern struct task_struct *processRegisteredForBallooning;

extern int my_int_len(int n);
extern void my_itoa(int n, char *buff);
int sigBalloonSent = 0;

pte_t* walk_the_page(unsigned long address) {
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep;
    struct mm_struct *mm = current->mm;

    pgd = pgd_offset(mm, (unsigned long)address);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        return NULL;
    }

    p4d = p4d_offset(pgd, (unsigned long)address);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        return NULL;
    }

    pud = pud_offset(p4d, (unsigned long)address);
    if(pud_none(*pud) || pud_bad(*pud)) {
        return NULL;
    }

    pmd = pmd_offset(pud, (unsigned long)address);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        return NULL;
    }

    ptep = pte_offset_map(pmd, (unsigned long)address);

    if (!pte_present(*ptep)) {
        return NULL;
    }

    return ptep;
}

int swap_my_pages(void *buff) {
    char swapFileName[64] = "/ballooning/swap_\0";
    char pid[8];
    int flags, i;
    struct file* swapFile;
    loff_t offset;

    my_itoa(processRegisteredForBallooning->pid, pid);
    strcat(swapFileName, pid);

	flags = O_RDWR;
    swapFile = filp_open(swapFileName, flags, 0);

    for (i = 0; i < SWAP_PAGES_COUNT; i++) {
        if (!(swappedOutPtes[i])) break;
    }
    if (i == SWAP_PAGES_COUNT) return 0;

    swappedOutPtes[i] = buff;
    offset = i * PAGE_SIZE;
    kernel_write(swapFile, buff, PAGE_SIZE, &offset);
    filp_close(swapFile, processRegisteredForBallooning->files);
    return 1;
}

void checkSigBalloon(void) {
    unsigned long freeMemoryKB = nr_free_pages() << (PAGE_SHIFT - 10); // - 10 to convert to KB
    if ((freeMemoryKB > FREE_MEMORY_LIMIT) && (sigBalloonSent == 1)) {
        sigBalloonSent = 0;
    }
}

int trying_to_swap_out(void *address) {
    pte_t *ptep, pte;
    struct page *page;
    int pagesSwappedOut;
    struct mm_struct *mm;
    
    mm = current->mm;

    ptep = walk_the_page((unsigned long)address);
    if (!ptep) return -1;

    pte = *ptep;

    page = pte_page(pte);
    pagesSwappedOut = swap_my_pages(address);

    if (pagesSwappedOut) {
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

        zap_page_range(vma, page_address_in_vma(page, vma), PAGE_SIZE);

        checkSigBalloon();
    } else {
        return -2;
    }

    return 0;
}

SYSCALL_DEFINE2(swap_pages, void *, buff, int, pages)
{
    // Printed once for every page suggested by user
    // Fill up the terminal, very
	// printk("Received system call [swap_pages] from process (pid: %d)\n", current->pid);

    return trying_to_swap_out(buff);
}

SYSCALL_DEFINE0(free_mem)
{
    return nr_free_pages() << (PAGE_SHIFT);
}
