#ifndef _JAILHOUSE_DRIVER_H
#define _JAILHOUSE_DRIVER_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct mem_region
{
	unsigned long long start;
	unsigned long long size;
};

struct jailhouse_enable_args
{
	struct mem_region hv_region;
	unsigned int rt_cpus;
};

#define JAILHOUSE_ENABLE _IOW(0, 0, struct jailhouse_enable_args)
#define JAILHOUSE_DISABLE _IO(0, 1)
#define JAILHOUSE_AXVM_CREATE _IOW(0, 0x11, struct axioctl_create_vm_arg)

// #define JAILHOUSE_BASE 0xffffff0000000000UL
#define JAILHOUSE_BASE 0xffffff8000000000UL
// #define JAILHOUSE_BASE 0xffffe00000000000UL
#define JAILHOUSE_SIGNATURE "EVMIMAGE"

/**
 * Hypervisor description.
 * Located at the beginning of the hypervisor binary image and loaded by
 * the driver (which also initializes some fields).
 */
struct jailhouse_header
{
	/** Signature "EVMIMAGE" used for basic validity check of the
	 * hypervisor image.
	 * @note Filled at build time. */
	char signature[8];
	/** Size of hypervisor core.
	 * It starts with the hypervisor's header and ends after its bss
	 * section. Rounded up to page boundary.
	 * @note Filled at build time. */
	unsigned long core_size;
	/** Size of the per-CPU data structure.
	 * @note Filled at build time. */
	unsigned long percpu_size;
	/** Entry point (arch_entry()).
	 * @note Filled at build time. */
	int (*entry)(unsigned int);
	/** Configured maximum logical CPU ID + 1.
	 * @note Filled by Linux loader driver before entry. */
	unsigned int max_cpus;
	/** Number of real-time CPUs paritioned, which will be shutdown before
	 * entry and restarted in hypervisor. The others are VM CPUs, which will
	 * call the entry function and run the guest.
	 * @note Filled by Linux loader driver before entry. */
	unsigned int rt_cpus;
};

#define JAILHOUSE_FILE_MAXNUM 8

struct jailhouse_preload_image
{
	__u64 source_address;
	__u64 size;
	__u64 target_address;
	__u64 padding;
};

struct axioctl_create_vm_arg
{
	// VM id
	__u64 vm_id;
	// Kernel image addr
	__u64 kernel_image_addr;
	// Kernel image size
	__u64 kernel_image_size;
	// Bios image addr
	__u64 bios_image_addr;
	// Bios image size
	__u64 bios_image_size;
	// Ramdisk image addr
	__u64 ramdisk_image_addr;
	// Ramdisk image size
	__u64 ramdisk_image_size;
	// Config file addr
	__u64 config_addr;
	// Config file size
	__u64 config_size;
};

#endif /* !_JAILHOUSE_DRIVER_H */
