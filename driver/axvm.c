#include <asm/cacheflush.h>
#include <linux/cpu.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/smp.h>

#include "axvm.h"
#include "hypercall.h"
#include "ioremap.h"

/// @brief Load image from user address to target physical address provided by
/// arceos-hv.
/// @param image : Here we reuse the jailhouse_preload_image structure from
/// Jailhouse.
///		image->source_address: user address.
///		image->size: image size.
///		image->target_address: target physical address provided by arceos-hv.
int arceos_axvm_load_image(struct jailhouse_preload_image *image)
{
	void *image_mem;
	int err = 0;

	__u64 page_offs, phys_start;

	phys_start = image->target_address & PAGE_MASK;
	page_offs = offset_in_page(image->target_address);

	pr_info("[%s]:\n", __func__);

	image_mem =
		jailhouse_ioremap(phys_start, 0, PAGE_ALIGN(image->size + page_offs));

	pr_info("phys_start 0x%llx remap to 0x%p\n", phys_start, image_mem);

	if (!image_mem)
	{
		pr_err(
			"jailhouse: Unable to map cell RAM at %08llx "
			"for image loading\n",
			(unsigned long long)(image->target_address));
		return -EBUSY;
	}

	pr_info(
		"copy to 0x%p size 0x%llx, loading...\n", image_mem + page_offs,
		image->size);

	if (copy_from_user(
			image_mem + page_offs,
			(void __user *)(unsigned long)image->source_address, image->size))
	{
		pr_err(
			"jailhouse: Unable to copy image from user %08llx "
			"for image loading\n",
			(unsigned long long)(image->source_address));
		err = -EFAULT;
	}

	/*
	 * ARMv7 and ARMv8 require to clean D-cache and invalidate I-cache for
	 * memory containing new instructions. On x86 this is a NOP.
	 */
	flush_icache_range(
		(unsigned long)(image_mem + page_offs),
		(unsigned long)(image_mem + page_offs) + image->size);
#ifdef CONFIG_ARM
	/*
	 * ARMv7 requires to flush the written code and data out of D-cache to
	 * allow the guest starting off with caches disabled.
	 */
	__cpuc_flush_dcache_area(image_mem + page_offs, image->size);
#endif

	vunmap(image_mem);

	return err;
}

/// @brief Create axvm config through HVC.
/// @param arg : Pointer to the user-provided VM creation information..
int arceos_cmd_axvm_create(struct axioctl_create_vm_arg __user *arg)
{
	struct axioctl_create_vm_arg vm_cfg;
	int err = 0;
	int vm_id = 0;

	unsigned long arg_phys_addr;
	struct axhvc_create_vm_arg *axhvc_axvm_create;
	struct jailhouse_preload_image bios_image;
	struct jailhouse_preload_image kernel_image;
	struct jailhouse_preload_image ramdisk_image;
	void *cfg_file_base;

	if (copy_from_user(&vm_cfg, arg, sizeof(vm_cfg)))
		return -EFAULT;

	axhvc_axvm_create = kmalloc(sizeof(struct axhvc_create_vm_arg), GFP_KERNEL);

	if (!axhvc_axvm_create)
	{
		pr_err("kmalloc for axhvc_create_vm_arg failed\n");
		return -ENOMEM;
	}

	axhvc_axvm_create->vm_id = 0;

	pr_err(
		"%s: cfg_file@ 0x%llx cfg_file_size 0x%llx\n", __func__,
		vm_cfg.config_addr, vm_cfg.config_size);

	// Allocate memory for config file
	cfg_file_base = kmalloc(vm_cfg.config_size, GFP_KERNEL);

	if (!cfg_file_base)
	{
		pr_err("kmalloc for cfg_file_base failed\n");
		err = -ENOMEM;
		goto error_free_arg;
	}

	if (copy_from_user(
			cfg_file_base, (void __user *)vm_cfg.config_addr,
			vm_cfg.config_size))
	{
		err = -EFAULT;
		pr_err("copy_from_user for cfg_file_base failed\n");
		goto error_free_cfg;
	}

	axhvc_axvm_create->cfg_file_gpa = __pa(cfg_file_base);
	axhvc_axvm_create->cfg_file_size = vm_cfg.config_size;

	axhvc_axvm_create->kernel_image_size = vm_cfg.kernel_image_size;
	axhvc_axvm_create->bios_image_size = vm_cfg.bios_image_size;
	axhvc_axvm_create->ramdisk_image_size = vm_cfg.ramdisk_image_size;

	pr_err(
		"%s: cfg_file_gpa 0x%llx, size 0x%llx\n", __func__,
		axhvc_axvm_create->cfg_file_gpa, axhvc_axvm_create->cfg_file_size);
	pr_err(
		"%s:\n\tkernel_image_size 0x%llx\n\tbios_image_size 0x%llx\n\t"
		"ramdisk_image_size 0x%llx\n",
		__func__, axhvc_axvm_create->kernel_image_size,
		axhvc_axvm_create->bios_image_size,
		axhvc_axvm_create->ramdisk_image_size);

	// These fields should be set by hypervisor.
	axhvc_axvm_create->kernel_load_gpa = 0xdeadbeef;
	axhvc_axvm_create->bios_load_gpa = 0xdeadbeef;
	axhvc_axvm_create->ramdisk_load_gpa = 0xdeadbeef;

	arg_phys_addr = __pa(axhvc_axvm_create);

	err = jailhouse_call_arg1(ARCEOS_HC_AXVM_CREATE_CFG, arg_phys_addr);
	if (err < 0)
	{
		pr_err("[%s] Failed in AXIOCTL_CREATE_VM\n", __func__);
		goto error_free_cfg;
	}

	pr_info(
		"[%s] AXIOCTL_CREATE_VM VM %d success\n", __func__,
		(int)axhvc_axvm_create->vm_id);
	pr_info(
		"[%s] VM [%d] bios_load_gpa 0x%llx\n", __func__,
		(int)axhvc_axvm_create->vm_id, axhvc_axvm_create->bios_load_gpa);
	pr_info(
		"[%s] VM [%d] kernel_load_gpa 0x%llx\n", __func__,
		(int)axhvc_axvm_create->vm_id, axhvc_axvm_create->kernel_load_gpa);
	pr_info(
		"[%s] VM [%d] ramdisk_load_gpa 0x%llx\n", __func__,
		(int)axhvc_axvm_create->vm_id, axhvc_axvm_create->ramdisk_load_gpa);
	vm_id = (int)axhvc_axvm_create->vm_id;

	vm_cfg.vm_id = vm_id;

	// Load kernel image
	if (vm_cfg.kernel_image_size != 0)
	{
		kernel_image.source_address = vm_cfg.kernel_image_addr;
		kernel_image.size = vm_cfg.kernel_image_size;
		kernel_image.target_address = axhvc_axvm_create->kernel_load_gpa;
		kernel_image.padding = 0;

		pr_info(
			"[%s] kernel_load_gpa: 0x%llx\n", __func__,
			axhvc_axvm_create->kernel_load_gpa);

		err = arceos_axvm_load_image(&kernel_image);
		if (err < 0)
		{
			pr_err(
				"[%s] Failed in arceos_axvm_load_image kernel_image\n",
				__func__);
			goto error_free_cfg;
		}
	}
	else
	{
		pr_err("No kernel image provided!\n");
		err = -EINVAL;
		goto error_free_cfg;
	}

	// Load BIOS image
	if (vm_cfg.bios_image_size > 0)
	{
		bios_image.source_address = vm_cfg.bios_image_addr;
		bios_image.size = vm_cfg.bios_image_size;
		bios_image.target_address = axhvc_axvm_create->bios_load_gpa;
		bios_image.padding = 0;

		pr_info(
			"[%s] bios_load_gpa: 0x%llx\n", __func__,
			axhvc_axvm_create->bios_load_gpa);

		err = arceos_axvm_load_image(&bios_image);
		if (err < 0)
		{
			pr_err(
				"[%s] Failed in arceos_axvm_load_image bios_image\n", __func__);
			goto error_free_cfg;
		}
	}

	// Load ramdisk image
	if (vm_cfg.ramdisk_image_size > 0)
	{
		ramdisk_image.source_address = vm_cfg.ramdisk_image_addr;
		ramdisk_image.size = vm_cfg.ramdisk_image_size;
		ramdisk_image.target_address = axhvc_axvm_create->ramdisk_load_gpa;
		ramdisk_image.padding = 0;

		pr_info(
			"[%s] ramdisk_load_gpa: 0x%llx\n", __func__,
			axhvc_axvm_create->ramdisk_load_gpa);

		err = arceos_axvm_load_image(&ramdisk_image);
		if (err < 0)
		{
			pr_err(
				"[%s] Failed in arceos_axvm_load_image ramdisk_image\n",
				__func__);
			goto error_free_cfg;
		}
	}

	pr_err("[%s] images load success, booting VM %d\n", __func__, vm_id);

	err = jailhouse_call_arg1(ARCEOS_HC_AXVM_BOOT, (unsigned long)vm_id);

	if (err < 0)
	{
		pr_err("[%s] Failed in AXIOCTL_BOOT_VM\n", __func__);
		goto error_free_cfg;
	}

	if (copy_to_user(arg, &vm_cfg, sizeof(vm_cfg)))
	{
		err = -EFAULT;
		goto error_free_cfg;
	}

	kfree(cfg_file_base);
	kfree(axhvc_axvm_create);

	return err;

error_free_cfg:
	kfree(cfg_file_base);

error_free_arg:
	pr_err("create axvm failed err:%d\n", err);
	kfree(axhvc_axvm_create);

	return err;
}
