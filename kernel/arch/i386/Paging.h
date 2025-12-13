#pragma once
#include <stdint.h>


typedef union __attribute__((packed))
{
	uint32_t value; /* full 32-bit word */

	struct __attribute__((packed))
	{
		uint32_t present : 1;		// bit 0
		uint32_t writable : 1;		// bit 1
		uint32_t user : 1;			// bit 2
		uint32_t write_through : 1; // bit 3
		uint32_t cache_disable : 1; // bit 4
		uint32_t accessed : 1;		// bit 5
		uint32_t dirty : 1;			// bit 6
		uint32_t pat : 1;			// bit 7
		uint32_t global : 1;		// bit 8
		uint32_t avail : 3;			// bits 9-11
		uint32_t phys_addr : 20;	// bits 12-31 (4-KiB frame)
	} bits;
} PageTableEntry;

/* 4-MiB (large) page entry — valid when PS=1 in PDE             *
 * Layout from Intel SDM Vol.3, Table 4-13 (32-bit paging).       */
typedef union __attribute__((packed))
{
	uint32_t value;

	struct __attribute__((packed))
	{
		uint32_t present : 1;		// bit 0  (P)
		uint32_t writable : 1;		// bit 1  (R/W)
		uint32_t user : 1;			// bit 2  (U/S)
		uint32_t write_through : 1; // bit 3  (PWT)
		uint32_t cache_disable : 1; // bit 4  (PCD)
		uint32_t accessed : 1;		// bit 5  (A)
		uint32_t dirty : 1;			// bit 6  (D) – set by CPU on write
		uint32_t page_size : 1;		// bit 7  (PS=1 for big page)
		union
		{
			struct
			{
				uint32_t global : 1;	 // bit 8  (G)
				uint32_t avail : 3;		 // bits 9-11 (OS-defined)
				uint32_t pat : 1;		 // bit 12 (PAT index bit 2)
				uint32_t reserved : 9;	 // bits 13-21 (must be 0)
				uint32_t phys_addr : 10; // bits 22-31 -> bits 32-41 of frame
			} goliath;
			struct
			{
				uint32_t avail : 4;		 // bits 9-11 (OS-defined)
				uint32_t phys_addr : 20; // bits 22-31 -> bits 32-41 of frame
				PageTableEntry *get_addr()
				{
					return (PageTableEntry *)(phys_addr << 12);
				}
			} david;
		} page_kind;
	} bits;
} PageDirectoryEntry;