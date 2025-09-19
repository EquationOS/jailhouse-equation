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
#define JAILHOUSE_AXVM_CREATE _IOW(0, 2, struct jailhouse_axvm_create)

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

#define JAILHOUSE_FILE_MAXNUM	8

struct jailhouse_preload_image {
	__u64 source_address;
	__u64 size;
	__u64 target_address;
	__u64 padding;
};

/**
 * Todo: We need to parse a cell configuration file similar to Jailhouse.
 * 	which can be reused.
 * 	This is just a ugly lazy implementation.
*/ 
struct jailhouse_axvm_create {
	// CPU MASK.
	__u64 cpu_mask;
	// VM_TYPE.
	__u32 type;
	// name_addr for each image.
	__u64 name_addr[JAILHOUSE_FILE_MAXNUM];
	// name_size for each image.
	__u64 name_size[JAILHOUSE_FILE_MAXNUM];
	// user addr for each image.
	__u64 img_addr[JAILHOUSE_FILE_MAXNUM];
	// size for each image.
	__u64 img_size[JAILHOUSE_FILE_MAXNUM];
};

#endif /* !_JAILHOUSE_DRIVER_H */
