/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PGALLOC_TRACK_H
#define _LINUX_PGALLOC_TRACK_H

#include <linux/mm.h>

extern struct mm_struct *init_mm_sym;

static typeof(__pte_alloc_kernel) *__pte_alloc_kernel_sym;
static typeof(pud_free_pmd_page) *pud_free_pmd_page_sym;
static typeof(pmd_set_huge) *pmd_set_huge_sym;
static typeof(pud_set_huge) *pud_set_huge_sym;
static typeof(pmd_free_pte_page) *pmd_free_pte_page_sym;
static typeof(__p4d_alloc) *__p4d_alloc_sym;
static typeof(__pud_alloc) *__pud_alloc_sym;
static typeof(__pmd_alloc) *__pmd_alloc_sym;
int jailhouse_ioremap_page_range(
	unsigned long addr, unsigned long end, phys_addr_t phys_addr, pgprot_t prot);

#if defined(CONFIG_MMU)
static inline p4d_t *p4d_get(struct mm_struct *mm, pgd_t *pgd,
				     unsigned long address,
				     pgtbl_mod_mask *mod_mask)
{
	if (unlikely(pgd_none(*pgd))) {
		if (__p4d_alloc_sym(mm, pgd, address))
			return NULL;
		*mod_mask |= PGTBL_PGD_MODIFIED;
	}

	return p4d_offset(pgd, address);
}

static inline pud_t *pud_get(struct mm_struct *mm, p4d_t *p4d,
				     unsigned long address,
				     pgtbl_mod_mask *mod_mask)
{
	if (unlikely(p4d_none(*p4d))) {
		if (__pud_alloc_sym(mm, p4d, address))
			return NULL;
		*mod_mask |= PGTBL_P4D_MODIFIED;
	}

	return pud_offset(p4d, address);
}

static inline pmd_t *pmd_get(struct mm_struct *mm, pud_t *pud,
				     unsigned long address,
				     pgtbl_mod_mask *mod_mask)
{
	if (unlikely(pud_none(*pud))) {
		if (__pmd_alloc_sym(mm, pud, address))
			return NULL;
		*mod_mask |= PGTBL_PUD_MODIFIED;
	}

	return pmd_offset(pud, address);
}
#endif /* CONFIG_MMU */


#define pte_alloc_kernel_track(pmd, address, mask)			\
	((unlikely(pmd_none(*(pmd))) &&					\
	  (__pte_alloc_kernel_sym(pmd) || ({*(mask)|=PGTBL_PMD_MODIFIED;0;})))?\
		NULL: pte_offset_kernel(pmd, address))

#endif /* _LINUX_PGALLOC_TRACK_H */
