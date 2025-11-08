#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/memory/PagingManager.h>

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
	void *end = (void *)((size_t)krn.memoryManager.buddyAllocator.m_p_block_info_array_start + krn.memoryManager.buddyAllocator.m_ui_block_info_array_size);
	size_t pdi = PAGE_DIR_INDEX((size_t)start);
	size_t pti = PAGE_TABLE_INDEX((size_t)start);
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
		printf("Reserved page: %x mapped to %x, %d, %d\n", start, pdi * 1024 + pti * 4096, pdi, pti);
		start = (void *)((size_t)start + 0x1000);
		pti++;
	}
}

int PagingManager::map_page(int pdi, int pti, void *phys_addr)
{
	PageTableEntry *pte = PAGE_TABLE_ENTRY_VIRTUAL_ADDR(pdi, pti);
	if (pte->value != 0)
		return -1;
	pte->value = (uint32_t)(phys_addr) | 3;
	return 0;
}
int PagingManager::identity_map_memory(void *start, void *end)
{
	size_t pdi = PAGE_DIR_INDEX((size_t)start);
	size_t pti = PAGE_TABLE_INDEX((size_t)start);
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
		auto pte = PAGE_TABLE_ENTRY_VIRTUAL_ADDR(pdi, pti);
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

int PagingManager::allocate_page(int dir_entry_idx, int table_entry_idx)
{
	void *addr = get_blank_page();
	if (!addr)
		return -1;
	return map_page(dir_entry_idx, table_entry_idx, addr);
}
int PagingManager::free_page(int dir_entry_idx, int table_entry_idx)
{
	PageTableEntry *pte = PAGE_TABLE_ENTRY_VIRTUAL_ADDR(dir_entry_idx, table_entry_idx);
	krn.memoryManager.buddyAllocator.free((void *)(pte->bits.phys_addr << 12));
	pte->value = 0;
	return 0;
}
int PagingManager::allocate_page_table(int dir_entry_idx)
{
	PageDirectoryEntry *dirEntry = PAGE_DIRECTORY_ENTRY_VIRTUAL_ADDR(dir_entry_idx);
	if (dirEntry->value != 0)
		return -1;
	void *addr = get_blank_page();
	if (!addr)
		return -1;
	dirEntry->value = (uint32_t)addr | 3;
	return 0;
}
int PagingManager::free_page_table(int dir_entry_idx)
{
	PageDirectoryEntry *dirEntry = PAGE_DIRECTORY_ENTRY_VIRTUAL_ADDR(dir_entry_idx);
	krn.memoryManager.buddyAllocator.free(dirEntry->bits.page_kind.david.get_addr());
	dirEntry->value = 0;
	return 0;
}

void PagingManager::find_free_pages(size_t num_pages, int *dir_entry_idx, int *table_entry_idx)
{
	
	size_t found = 0;

	found:
	
	return;
	not_found:
	*dir_entry_idx = -1;
	*table_entry_idx = -1;
	return;
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
	*(((uint32_t*)pageDirectory)+1023) = (uint32_t)pageDirectory | 3;

}
void PagingManager::free(void *addr, size_t num_pages)
{
	int pdi = PAGE_DIR_INDEX((size_t)addr);
	int pti = PAGE_TABLE_INDEX((size_t)addr);
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
		return nullptr;
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
			break;
		pti_counter++;
	}
	if (i != num_pages)
	{
		free(pdi, pti, i);
		return nullptr;
	}
	return (void *)PAGE_ADDR(pdi, pti);
}
