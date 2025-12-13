#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/memory/PagingManager.h>


extern "C" void loadPageDirectory(unsigned int *);
extern "C" void enablePaging();


constexpr uint32_t PAGE_DIRECTORY_VIRTUAL_ADDR = 0xfffff000;
constexpr uint32_t START_OF_PAGE_TABLES = 0xffc00000;
constexpr uint32_t PAGE_TABLE_ENTRIES = 1024;
constexpr uint32_t PAGE_DIRECTORY_ENTRIES = 1024;

inline constexpr PageDirectoryEntry *page_directory_entry_virtual_addr(int pdi)
{
	return reinterpret_cast<PageDirectoryEntry *>(PAGE_DIRECTORY_VIRTUAL_ADDR + (pdi << 2));
}

inline constexpr PageTableEntry *page_table_virtual_addr(int pdi)
{
	return reinterpret_cast<PageTableEntry *>(START_OF_PAGE_TABLES + (pdi << 12));
}

inline constexpr PageTableEntry *page_table_entry_virtual_addr(int pdi, int pti)
{
	return reinterpret_cast<PageTableEntry *>(reinterpret_cast<size_t>(page_table_virtual_addr(pdi)) + (pti << 2));
}

inline constexpr uint32_t page_addr(int pdi, int pti)
{
	return ((pdi) << 22) | ((pti) << 12);
}

inline constexpr int page_dir_index(uint32_t addr)
{
	return ((addr) >> 22) & 0x3FF;
}

inline constexpr int page_table_index(uint32_t addr)
{
	return ((addr) >> 12) & 0x3FF;
}

void PagingManager::init()
{
	pageDirectory = (PageDirectoryEntry *)get_blank_page();
	if (pageDirectory == nullptr)
	{
		abort();
	}
	memset(pageDirectory, 0, PAGE_TABLE_ENTRIES * sizeof(int));
	map_kernel_memory();

	// Map the last page directory entry to itself
	// ((volatile PageDirectoryEntry *)pageDirectory)[1023].value = (uint32_t)pageDirectory | 3;
	*(((uint32_t *)pageDirectory) + 1023) = (uint32_t)pageDirectory | 3;
}

void PagingManager::free(void *addr, size_t num_pages)
{
	int pdi = page_dir_index((size_t)addr);
	int pti = page_table_index((size_t)addr);

	free(pdi, pti, num_pages);
}

void PagingManager::free(int pdi, int pti, size_t num_pages)
{
	int pdi_counter = pdi, pti_counter = pti;
	size_t i = 0;

	for (; i < num_pages; i++)
	{
		if (pti_counter >= PAGE_TABLE_ENTRIES)
		{
			pdi_counter++;
			pti_counter = 0;
			// free_page_table(dir_entry_idx); --- TODO ---
		}
		if (free_page(pdi_counter, pti_counter) != 0)
			break;
		pti_counter++;
	}
}

void *PagingManager::allocate(size_t num_pages)
{
	int pdi = -1, pti = -1;
	find_free_pages(num_pages, &pdi, &pti);
	int pdi_counter = pdi, pti_counter = pti;
	if (pdi == -1 || pti == -1)
	{
		return nullptr;
	}

	size_t i = 0;
	for (; i < num_pages; i++)
	{
		if (pti_counter >= PAGE_TABLE_ENTRIES)
		{
			pdi_counter++;
			pti_counter = 0;
			// allocate_page_table(dir_entry_idx); --- TODO ---
		}
		if (allocate_page(pdi_counter, pti_counter) != 0)
		{
			break;
		}

		pti_counter++;
	}

	if (i != num_pages)
	{
		free(pdi, pti, i);
		return nullptr;
	}

	return (void *)page_addr(pdi, pti);
}

void *PagingManager::map(void *phys_addr)
{
	int dir_entry_idx = -1, table_entry_idx = -1;
	find_free_pages(1, &dir_entry_idx, &table_entry_idx);
	if (dir_entry_idx == -1 || table_entry_idx == -1)
		return nullptr;
	if (map_page(dir_entry_idx, table_entry_idx, phys_addr) != 0)
		return nullptr;

	return (void *)page_addr(dir_entry_idx, table_entry_idx);
}

void PagingManager::load()
{
	loadPageDirectory((unsigned int *)pageDirectory);
}

void PagingManager::flush_tlb()
{
	uint32_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	asm volatile("mov %0, %%cr3" ::"r"(cr3) : "memory");
}

void PagingManager::flush_tlb_single(uint32_t vaddr)
{
	asm volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

void PagingManager::enable_paging()
{
	::enablePaging();
}

void *PagingManager::get_blank_page()
{
	void *addr = krn.memoryManager.buddyAllocator.allocate(BUDDY_ALLOCATOR_MIN_BLOCK_SIZE);
	if (!addr)
		return nullptr;
	// memset(addr, 0, BUDDY_ALLOCATOR_MIN_BLOCK_SIZE);
	return addr;
}

void PagingManager::map_kernel_memory()
{
	// Must be called before enabling paging
	void *start = (void *)(krn.memoryManager.p_kernel_start);
	void *end = (void *)((size_t)krn.memoryManager.buddyAllocator._m_p_block_info_array_start + krn.memoryManager.buddyAllocator._m_ui_block_info_array_size);
	size_t pdi = page_dir_index((size_t)start);
	size_t pti = page_table_index((size_t)start);
	PageTableEntry *pageTable;
	goto l_newPageTable;
	while (start < end)
	{

		if (pti >= PAGE_TABLE_ENTRIES)
		{
			pdi++;
			pti = 0;
		l_newPageTable:
			pageTable = (PageTableEntry *)get_blank_page();
			memset(pageTable, 0, PAGE_TABLE_ENTRIES * sizeof(PageTableEntry));
			pageDirectory[pdi].value = (uint32_t)pageTable | 3;
		}
		pageTable[pti].value = (uint32_t)start | 3;
		start = (void *)((size_t)start + 0x1000);
		pti++;
	}
}

void PagingManager::find_free_pages(size_t num_pages, int *dir_entry_idx, int *table_entry_idx)
{
	PageDirectoryEntry *dirEntry;
	size_t current_found_size = 0;

	for (size_t pdi = 0; pdi < PAGE_DIRECTORY_ENTRIES; pdi++)
	{
		dirEntry = page_directory_entry_virtual_addr(pdi);
		if (dirEntry->value == 0)
		{
			if (current_found_size == 0)
			{
				*dir_entry_idx = pdi;
				*table_entry_idx = 0;
			}
			// Page table not allocated, so all entries are free
			if (num_pages <= PAGE_TABLE_ENTRIES)
			{
				current_found_size += PAGE_TABLE_ENTRIES;
			}
		}
		else
		{
			PageTableEntry *tableEntry = page_table_entry_virtual_addr(pdi, 0);
			for (size_t pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
			{
				if (tableEntry[pti].value == 0)
				{
					if (current_found_size == 0)
					{
						*dir_entry_idx = pdi;
						*table_entry_idx = pti;
					}
					current_found_size++;
				}
				else
				{
					current_found_size = 0;
				}
			}
		}

		if (current_found_size >= num_pages)
		{
			goto found;
		}
	}

not_found:
	*dir_entry_idx = -1;
	*table_entry_idx = -1;
	return;

found:
	return;
}

int PagingManager::allocate_page(int dir_entry_idx, int table_entry_idx)
{
	void *addr = get_blank_page();
	if (!addr)
		return -1;
	return map_page(dir_entry_idx, table_entry_idx, addr);
}

int PagingManager::allocate_page_table(int dir_entry_idx)
{
	PageDirectoryEntry *dirEntry = page_directory_entry_virtual_addr(dir_entry_idx);
	if (dirEntry->value != 0)
	{
		return -1;
	}

	void *addr = get_blank_page();
	if (!addr)
	{
		return -1;
	}

	dirEntry->value = (uint32_t)addr | 3;

	return 0;
}

int PagingManager::free_page_table(int dir_entry_idx)
{
	PageDirectoryEntry *dirEntry = page_directory_entry_virtual_addr(dir_entry_idx);
	krn.memoryManager.buddyAllocator.free(dirEntry->bits.page_kind.david.get_addr());
	dirEntry->value = 0;
	return 0;
}

int PagingManager::free_page(int dir_entry_idx, int table_entry_idx)
{
	PageTableEntry *pte = page_table_entry_virtual_addr(dir_entry_idx, table_entry_idx);
	krn.memoryManager.buddyAllocator.free((void *)(pte->bits.phys_addr << 12));
	pte->value = 0;

	return 0;
}

int PagingManager::map_page(int pdi, int pti, void *phys_addr)
{
	PageTableEntry *pte = page_table_entry_virtual_addr(pdi, pti);
	if (pte->value != 0)
		return -1;
	pte->value = (uint32_t)(phys_addr) | 3;

	return 0;
}

int PagingManager::identity_map_memory(void *start, void *end)
{
	size_t pdi = page_dir_index((size_t)start);
	size_t pti = page_table_index((size_t)start);

	PageTableEntry *pageTable;

	goto l_newPageTable;

	while (start < end)
	{

		if (pti >= PAGE_TABLE_ENTRIES)
		{
			pdi++;
			pti = 0;
		l_newPageTable:
			allocate_page_table(pdi);
		}
		auto pte = page_table_entry_virtual_addr(pdi, pti);
		if (pte->value != 0)
			// TODO: unmap previous mapping
			return -1;
		pte->value = (uint32_t)start | 3;
		start = (void *)((size_t)start + 0x1000);
		flush_tlb_single((uint32_t)start);
		pti++;
	}

	return 0;
}
