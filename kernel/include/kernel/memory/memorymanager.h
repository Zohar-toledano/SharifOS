#pragma once

#include <cstddef>
#include <stdint.h>
#include <multiboot2.h>
#include <kernel/memory/BuddyAllocator.h>
#include <kernel/memory/PagingManager.h>

class BuddyAllocator; // Forward declaration

class MemoryManager
{
public:
	void init(uintptr_t p_multiboot_info);

private:
	void set_physicalmemory_dimensions();
	void set_kernel_dimensions();
	struct multiboot_tag_elf_sections *get_elf_sections();

public:
	uintptr_t p_multiboot_info;
	uintptr_t ul_memory_start;
	uint64_t ul_physical_memory_size;
	uint64_t ul_kernel_size;
	uintptr_t p_kernel_start;

public:
	BuddyAllocator buddyAllocator;
	PagingManager pagingManager;
};
