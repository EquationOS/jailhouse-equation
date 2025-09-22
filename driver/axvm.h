#ifndef _JAILHOUSE_DRIVER_AXVM_H
#define _JAILHOUSE_DRIVER_AXVM_H

#include <linux/cpumask.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/uaccess.h>

#include "jailhouse.h"

#define ARCEOS_HC_AXVM_CREATE_CFG 0x101
#define ARCEOS_HC_AXVM_BOOT 0x102

/** The struct used for parameter passing between the kernel module and ArceOS
 * hypervisor. This structure should have the same memory layout as the
 * `AxVMCreateArg` structure in ArceOS. See arceos/modules/axvm/src/hvc.rs
 */
struct axhvc_create_vm_arg
{
	// VM ID, set by ArceOS hypervisor.
	__u64 vm_id;

	// Configuration file loaded guest physical address.
	__u64 cfg_file_gpa;
	// Configuration file size.
	__u64 cfg_file_size;

	// Following fields should be set by AxVisor.

	// Kernel image loaded target guest physical address.
	__u64 kernel_load_gpa;
	// BIOS image loaded target guest physical address.
	__u64 bios_load_gpa;
	// Ramdisk image loaded target guest physical address.
	__u64 ramdisk_load_gpa;
};

int arceos_axvm_load_image(struct jailhouse_preload_image *image);

int arceos_cmd_axvm_create(struct jailhouse_axvm_create __user *arg);

#endif /* !_JAILHOUSE_DRIVER_AXVM_H */