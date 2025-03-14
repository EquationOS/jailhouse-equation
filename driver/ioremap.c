#include <linux/hugetlb.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/mmzone.h>
#include <linux/pgtable.h>
#include <linux/vmalloc.h>

#include "ioremap.h"

#ifdef CONFIG_HAVE_ARCH_HUGE_VMAP
static unsigned int ioremap_max_page_shift = BITS_PER_LONG - 1;
#else  /* CONFIG_HAVE_ARCH_HUGE_VMAP */
static const unsigned int ioremap_max_page_shift = PAGE_SHIFT;
#endif /* CONFIG_HAVE_ARCH_HUGE_VMAP */

struct mm_struct *init_mm_sym;

typeof(__pte_alloc_kernel) *__pte_alloc_kernel_sym;
typeof(pud_free_pmd_page) *pud_free_pmd_page_sym;
typeof(pmd_set_huge) *pmd_set_huge_sym;
typeof(pud_set_huge) *pud_set_huge_sym;
typeof(pmd_free_pte_page) *pmd_free_pte_page_sym;
typeof(__p4d_alloc) *__p4d_alloc_sym;
typeof(__pud_alloc) *__pud_alloc_sym;
typeof(__pmd_alloc) *__pmd_alloc_sym;

/*** Page table manipulation functions ***/
static int vmap_pte_range(
	pmd_t *pmd, unsigned long addr, unsigned long end, phys_addr_t phys_addr,
	pgprot_t prot, unsigned int max_page_shift, pgtbl_mod_mask *mask)
{
	pte_t *pte;
	u64 pfn;
	struct page *page;
	unsigned long size = PAGE_SIZE;

	pr_err("[JAILHOUSE] vmap_pte_range: 0x%lx - 0x%lx\n", addr, end);
	pr_err("pmd: %p\n", pmd);

	pfn = phys_addr >> PAGE_SHIFT;
	pte = pte_alloc_kernel_track(pmd, addr, mask);

	pr_err("[JAILHOUSE] vmap_pte_range: pfn 0x%llx\n", pfn);
	pr_err("pte: %p\n", pte);

	if (!pte)
		return -ENOMEM;
	do
	{
		if (unlikely(!pte_none(ptep_get(pte))))
		{
			if (pfn_valid(pfn))
			{
				page = pfn_to_page(pfn);
				dump_page(page, "remapping already mapped page");
			}
			BUG();
		}

#ifdef CONFIG_HUGETLB_PAGE
		size = arch_vmap_pte_range_map_size(addr, end, pfn, max_page_shift);
		if (size != PAGE_SIZE)
		{
			pte_t entry = pfn_pte(pfn, prot);

			entry = arch_make_huge_pte(entry, ilog2(size), 0);
			set_huge_pte_at(init_mm_sym, addr, pte, entry, size);
			pfn += PFN_DOWN(size);
			continue;
		}
#endif
		set_pte_at(init_mm_sym, addr, pte, pfn_pte(pfn, prot));
		pfn++;
	} while (pte += PFN_DOWN(size), addr += size, addr != end);
	*mask |= PGTBL_PTE_MODIFIED;
	return 0;
}

static int vmap_try_huge_pmd(
	pmd_t *pmd, unsigned long addr, unsigned long end, phys_addr_t phys_addr,
	pgprot_t prot, unsigned int max_page_shift)
{
	if (max_page_shift < PMD_SHIFT)
		return 0;

	if (!arch_vmap_pmd_supported(prot))
		return 0;

	if ((end - addr) != PMD_SIZE)
		return 0;

	if (!IS_ALIGNED(addr, PMD_SIZE))
		return 0;

	if (!IS_ALIGNED(phys_addr, PMD_SIZE))
		return 0;

	if (pmd_present(*pmd) && !pmd_free_pte_page_sym(pmd, addr))
		return 0;

	return pmd_set_huge_sym(pmd, phys_addr, prot);
}

static int vmap_pmd_range(
	pud_t *pud, unsigned long addr, unsigned long end, phys_addr_t phys_addr,
	pgprot_t prot, unsigned int max_page_shift, pgtbl_mod_mask *mask)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_alloc_track(init_mm_sym, pud, addr, mask);

	pr_err("[JAILHOUSE] vmap_pmd_range: 0x%lx - 0x%lx\n", addr, end);
	pr_err("pmd: %p\n", pmd);

	unsigned long index = pmd_index(addr);
	pr_err("pmd[%ld] %lx\n", index, pmd[index].pmd);

	if (!pmd)
		return -ENOMEM;
	do
	{
		next = pmd_addr_end(addr, end);
		pr_err("before map pmd_val: %lx ", pmd_val(*pmd));

		unsigned long index = pmd_index(addr);
		pr_err("pmd[%ld] %lx\n", index, pmd[index].pmd);

		if (vmap_try_huge_pmd(pmd, addr, next, phys_addr, prot, max_page_shift))
		{
			*mask |= PGTBL_PMD_MODIFIED;

			pr_err(
				"[JAILHOUSE] address: [0x%lx-0x%lx] mapped to 0x%llx as huge\n",
				addr, next, phys_addr);
			pr_err("after map pmd_val: %lx ", pmd_val(*pmd));
			index = pmd_index(addr);
			pr_err("pmd[%ld] %lx\n", index, pmd[index].pmd);
			continue;
		}

		if (vmap_pte_range(
				pmd, addr, next, phys_addr, prot, max_page_shift, mask))
			return -ENOMEM;

		index = pmd_index(addr);
		pr_err("after map pmd[%ld] %lx\n", index, pmd[index].pmd);
	} while (pmd++, phys_addr += (next - addr), addr = next, addr != end);
	return 0;
}

static int vmap_try_huge_pud(
	pud_t *pud, unsigned long addr, unsigned long end, phys_addr_t phys_addr,
	pgprot_t prot, unsigned int max_page_shift)
{
	if (max_page_shift < PUD_SHIFT)
		return 0;

	if (!arch_vmap_pud_supported(prot))
		return 0;

	if ((end - addr) != PUD_SIZE)
		return 0;

	if (!IS_ALIGNED(addr, PUD_SIZE))
		return 0;

	if (!IS_ALIGNED(phys_addr, PUD_SIZE))
		return 0;

	if (pud_present(*pud) && !pud_free_pmd_page_sym(pud, addr))
		return 0;

	return pud_set_huge_sym(pud, phys_addr, prot);
}

static int vmap_pud_range(
	p4d_t *p4d, unsigned long addr, unsigned long end, phys_addr_t phys_addr,
	pgprot_t prot, unsigned int max_page_shift, pgtbl_mod_mask *mask)
{
	pud_t *pud;
	unsigned long next;
	pud = pud_alloc_track(init_mm_sym, p4d, addr, mask);

	pr_err("[JAILHOUSE] vmap_pud_range: 0x%lx - 0x%lx\n", addr, end);
	pr_err("pud: %p\n", pud);

	if (!pud)
		return -ENOMEM;
	do
	{
		next = pud_addr_end(addr, end);

		unsigned long index = pud_index(addr);
		pr_err("before map pud[%ld] %lx\n", index, pud[index].pud);

		if (vmap_try_huge_pud(pud, addr, next, phys_addr, prot, max_page_shift))
		{
			*mask |= PGTBL_PUD_MODIFIED;
			continue;
		}

		if (vmap_pmd_range(
				pud, addr, next, phys_addr, prot, max_page_shift, mask))
			return -ENOMEM;

		index = pud_index(addr);
		pr_err("after map pud[%ld] %lx\n", index, pud[index].pud);
	} while (pud++, phys_addr += (next - addr), addr = next, addr != end);
	return 0;
}

static int vmap_try_huge_p4d(
	p4d_t *p4d, unsigned long addr, unsigned long end, phys_addr_t phys_addr,
	pgprot_t prot, unsigned int max_page_shift)
{
	if (max_page_shift < P4D_SHIFT)
		return 0;

	if (!arch_vmap_p4d_supported(prot))
		return 0;

	if ((end - addr) != P4D_SIZE)
		return 0;

	if (!IS_ALIGNED(addr, P4D_SIZE))
		return 0;

	if (!IS_ALIGNED(phys_addr, P4D_SIZE))
		return 0;

	if (p4d_present(*p4d) && !p4d_free_pud_page(p4d, addr))
		return 0;

	return p4d_set_huge(p4d, phys_addr, prot);
}

static int vmap_p4d_range(
	pgd_t *pgd, unsigned long addr, unsigned long end, phys_addr_t phys_addr,
	pgprot_t prot, unsigned int max_page_shift, pgtbl_mod_mask *mask)
{
	p4d_t *p4d;
	unsigned long next;

	p4d = p4d_alloc_track(init_mm_sym, pgd, addr, mask);

	pr_err("[JAILHOUSE] vmap_p4d_range: 0x%lx - 0x%lx\n", addr, end);
	pr_err("p4d: %p\n", p4d);

	unsigned long index = p4d_index(addr);
	pr_err("p4d[%ld] %lx\n", index, p4d[index].p4d);

	if (!p4d)
		return -ENOMEM;
	do
	{
		next = p4d_addr_end(addr, end);

		if (vmap_try_huge_p4d(p4d, addr, next, phys_addr, prot, max_page_shift))
		{
			*mask |= PGTBL_P4D_MODIFIED;
			continue;
		}

		if (vmap_pud_range(
				p4d, addr, next, phys_addr, prot, max_page_shift, mask))
			return -ENOMEM;
	} while (p4d++, phys_addr += (next - addr), addr = next, addr != end);
	return 0;
}

// static inline unsigned long __native_read_cr3(void)
// {
// 	unsigned long val;
// 	asm volatile("mov %%cr3,%0\n\t" : "=r" (val) : __FORCE_ORDER);
// 	return val;
// }

static int vmap_range_noflush(
	unsigned long addr, unsigned long end, phys_addr_t phys_addr, pgprot_t prot,
	unsigned int max_page_shift)
{
	pgd_t *pgd;
	pgd_t *pgd_new;
	unsigned long start;
	unsigned long next;
	int err;
	pgtbl_mod_mask mask = 0;

	might_sleep();
	BUG_ON(addr >= end);

	pgd_t *base = __va(read_cr3_pa());
	pgd_t *cr3_pgd = &base[pgd_index(addr)];

	int count = 512;

	// pr_err("pgd: \n");

	start = addr;
	pgd = pgd_offset(init_mm_sym, addr);
	pgd_new = pgd;
	unsigned long index = 0;

	pr_err(
		"%ld: cr3_pgd: %lx , pdg: %lx\n", index, cr3_pgd[index].pgd,
		pgd_new[index].pgd);

	index = pgd_index(addr);
	// pr_err("cr3_pgd: \n");
	pr_err(
		"%ld: cr3_pgd: %lx , pdg: %lx\n", index, cr3_pgd[index].pgd,
		pgd_new[index].pgd);

	// for(int i = 0; i < count; i++) {
	// 	pr_err("%lx ", );
	// }

	pr_err("[JAILHOUSE] vmap_range_noflush: 0x%lx - 0x%lx\n", addr, end);
	pr_err(
		"pgd at %lx: 0x%lx value 0x%lx\n", (unsigned long)pgd, pgd->pgd,
		(unsigned long)pgd_val(*pgd));

	pr_err("pgd pa %lx\n", __pa((unsigned long)pgd));

	pr_err("init_mm pgd %lx\n", __pa((unsigned long)init_mm_sym->pgd));

	// pr_err("swapper_pg_dir %lx\n", __pa((unsigned long) swapper_pg_dir));

	pr_err("cr3 0x%lx\n", __native_read_cr3());

	do
	{
		next = pgd_addr_end(addr, end);
		err = vmap_p4d_range(
			pgd, addr, next, phys_addr, prot, max_page_shift, &mask);
		if (err)
			break;
	} while (pgd++, phys_addr += (next - addr), addr = next, addr != end);

	pr_err("map finished: \n");
	for (int i = 0; i < count; i++)
	{
		if (cr3_pgd[i].pgd != pgd_new[i].pgd)
			pr_err(
				"%d: cr3_pgd: %lx , pdg: %lx\n", i, cr3_pgd[i].pgd,
				pgd_new[i].pgd);
	}

	if (mask & ARCH_PAGE_TABLE_SYNC_MASK)
		arch_sync_kernel_mappings(start, end);

	return err;
}

int jailhouse_ioremap_page_range(
	unsigned long addr, unsigned long end, phys_addr_t phys_addr, pgprot_t prot)
{
	int err;

	err =
		vmap_range_noflush(addr, end, phys_addr, prot, ioremap_max_page_shift);
	flush_cache_vmap(addr, end);
	if (!err)
		err = kmsan_ioremap_page_range(
			addr, end, phys_addr, prot, ioremap_max_page_shift);
	return err;
}