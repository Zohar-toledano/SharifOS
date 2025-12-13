#pragma once
#include <stdint.h>
#include <arch/Paging.h>

extern "C" void loadPageDirectory(unsigned int *);
extern "C" void enablePaging();

class PagingManager
{
	friend class MemoryManager;

private:
	PageDirectoryEntry *pageDirectory;

private:
	void *get_blank_page();
	void map_kernel_memory();
	int identity_map_memory(void *start, void *end);
	int allocate_page(int dir_entry_idx, int table_entry_idx);
	int free_page(int dir_entry_idx, int table_entry_idx);
	int allocate_page_table(int dir_entry_idx);
	int free_page_table(int dir_entry_idx);
	void find_free_pages(size_t num_pages, int *dir_entry_idx, int *table_entry_idx);
	int map_page(int pdi, int pti, void *phys_addr);

public:
	void init();
	void free(void *addr, size_t num_pages);
	void free(int pdi, int pti, size_t num_pages);
	void *allocate(size_t num_pages);
	void *map(void *phys_addr);
	inline void load()
	{
		loadPageDirectory((unsigned int *)pageDirectory);
	}

private:
	static inline void enablePaging()
	{
		::enablePaging();
	}

public:
	static inline void flush_tlb_single(uint32_t vaddr)
	{
		asm volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
	}

	inline void flush_tlb()
	{
		uint32_t cr3;
		asm volatile("mov %%cr3, %0" : "=r"(cr3));
		asm volatile("mov %0, %%cr3" ::"r"(cr3) : "memory");
	}
};
