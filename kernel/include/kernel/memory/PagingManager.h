#pragma once

#include <arch/Paging.h>

class PagingManager
{
	friend class MemoryManager;

public:
	void init();
	void free(void *addr, size_t num_pages);
	void free(int pdi, int pti, size_t num_pages);
	void *allocate(size_t num_pages);
	void *map(void *phys_addr);
	void load();

public:
	static void flush_tlb();
	static void flush_tlb_single(uint32_t vaddr);
	static void enable_paging();

private:
	void *get_blank_page();
	void map_kernel_memory();
	void find_free_pages(size_t num_pages, int *dir_entry_idx, int *table_entry_idx);
	int allocate_page(int dir_entry_idx, int table_entry_idx);
	int allocate_page_table(int dir_entry_idx);
	int free_page_table(int dir_entry_idx);
	int free_page(int dir_entry_idx, int table_entry_idx);
	int map_page(int pdi, int pti, void *phys_addr);
	int identity_map_memory(void *start, void *end);

private:
	PageDirectoryEntry *pageDirectory;
};
