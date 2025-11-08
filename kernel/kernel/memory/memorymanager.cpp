#include <kernel/memory/memorymanager.h>
#include <kernel/tty.h>
#define ALIGN_UP(addr) (((addr) + 7) & ~7)

void MemoryManager::set_physicalmemory_dimensions()
{
	uint64_t total_ram_bytes = 0;
	multiboot_tag_mmap *mmap_tag = NULL;

	// The first tag is 8 bytes after the start of the MBI structure.
	multiboot_tag *tag = (multiboot_tag *)(p_multiboot_info + 8);

	// Loop through all the tags
	while (tag->type != 0)
	{ // Type 0 is the end tag
		if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
		{ // Type 6 is the Memory Map tag
			mmap_tag = (multiboot_tag_mmap *)tag;
			break; // Found it, exit the loop
		}

		// Advance to the next tag. The size must be 8-byte aligned.
		tag = (multiboot_tag *)((uint8_t *)tag + ALIGN_UP(tag->size));
	}

	if (mmap_tag == NULL)
	{
		// Handle error: Memory map not found!
		// You cannot proceed with memory management.
		// print_error("Memory map not provided by bootloader!");
		ul_physical_memory_size = 0; // No memory map found, set RAM size to 0
		return;
	}

	// Now, iterate through the memory map entries
	uint32_t entry_size = mmap_tag->entry_size;
	uint32_t num_entries = (mmap_tag->size - sizeof(multiboot_tag_mmap)) / entry_size;

	multiboot_mmap_entry *mmap_entry = (multiboot_mmap_entry *)((uint8_t *)mmap_tag + sizeof(multiboot_tag_mmap));

	for (uint32_t i = 0; i < num_entries; ++i)
	{
		// Check if the memory region is available RAM (type 1)
		if (mmap_entry->type == 1)
		{
			total_ram_bytes += mmap_entry->len;
		}

		// Advance to the next entry
		mmap_entry = (multiboot_mmap_entry *)((uint8_t *)mmap_entry + entry_size);
	}
	// 'total_ram_bytes' now holds the total amount of usable RAM.
	ul_physical_memory_size = total_ram_bytes; // Store the total RAM size in the class member
}

struct multiboot_tag_elf_sections *MemoryManager::get_elf_sections()
{
	struct multiboot_tag *tag;
	struct multiboot_tag_elf_sections *elf = NULL;

	for (tag = (struct multiboot_tag *)(p_multiboot_info + 8); tag->type != MULTIBOOT_TAG_TYPE_END; tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
	{
		if (tag->type == MULTIBOOT_TAG_TYPE_ELF_SECTIONS)
		{
			elf = (struct multiboot_tag_elf_sections *)tag;
			break;
		}
	}
	return elf;
}

void MemoryManager::set_kernel_dimensions()
{
	struct multiboot_tag_elf_sections *elf = get_elf_sections();

	uint64_t min_addr = UINT64_MAX;
	uint64_t max_addr = 0;

	// Pointer to first section header
	struct multiboot_elf_section_header *shdr = (struct multiboot_elf_section_header *)(elf + 1);

	for (uint32_t i = 0; i < elf->num; i++)
	{

		// Pointer to i-th section header, using elf->entsize
		uint8_t *entry = (uint8_t *)elf->sections + i * elf->entsize;

		// Read addr and size from known ELF32 offsets
		uint32_t addr = *(uint32_t *)(entry + 12); // offset of `addr` in ELF32
		uint32_t size = *(uint32_t *)(entry + 20); // offset of `size` in ELF32

		if (size == 0)
			continue;

		if (addr < min_addr)
			min_addr = addr;

		if (addr + size > max_addr)
			max_addr = addr + size;
	}
	ul_kernel_size = max_addr - min_addr;
	p_kernel_start = min_addr; // Set the kernel start address
}

void MemoryManager::init(uintptr_t p_multiboot_info)
{
	this->p_multiboot_info = p_multiboot_info;
	set_physicalmemory_dimensions();
	set_kernel_dimensions();
	buddyAllocator.init();
	pagingManager.init();
	pagingManager.load();
	pagingManager.enablePaging();

	void* vga_addr = buddyAllocator.allocate(1,VGA_MEMORY);
	pagingManager.identity_map_memory(vga_addr, (void*)((size_t)vga_addr + 25*80*2*2));
	pagingManager.flush_tlb();
}
