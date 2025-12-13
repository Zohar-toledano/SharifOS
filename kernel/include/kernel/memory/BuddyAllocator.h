#pragma once
#include <consts.h>
#include <stdint.h>
#include <stdio.h>

#define ALIGN_UP_4K(addr) (((addr) + 0xFFF) & ~0xFFF)
#define IS_ALIGNED(addr, alignment) (((uintptr_t)(addr) & ((alignment) - 1)) == 0)
class MemoryManager; // Forward declaration

#pragma pack(push, 1) // Ensure 1-byte alignment for the BuddyBlock structure

/**
 * @struct list_head
 * @brief Represents a node in a doubly-linked list.
 *
 * Used to link free or used page blocks in the buddy allocator system.
 */
struct list_head
{
	void *next;
	void *prev;
};

/**
 * @struct page
 * @brief Metadata structure representing a single memory page in the buddy allocator.
 *
 * Contains status flags, order, and links to free list. This struct is used to track
 * memory usage and organize blocks of memory for allocation and deallocation.
 */
struct page
{
	uint8_t flags; // |?|?|?|?|?|?|?|inuse|
	uint8_t order;
	struct list_head lru;

	inline void setUsed();
	inline void setUnused();
	inline bool isUsed();
	inline void setChained();
	inline void clearChained();
	inline bool isChained() const;
	inline void *getBlockStart();
};
#pragma pack(pop) // Restore previous alignment

/**
 * @brief Buddy memory allocator for managing dynamic memory allocations.
 *
 * Implements the buddy allocation algorithm with multiple _orders of page blocks.
 * Provides allocation, deallocation, and internal utilities for managing free blocks.
 */
class BuddyAllocator
{
	friend class page;

public:
	void init();
	void reserved_memory(void *start, void *end);
	void *allocate(size_t size);
	void *allocate(uint8_t desiredOrder, void *preferredAddr);
	bool free(void *ptr);

private:
	int find_closest_lower_order(size_t size);
	size_t find_closest_upper_order(size_t size);
	size_t get_free_blocks_len(uint8_t order);
	void remove_node_from_list(page *node, uint8_t order);
	void add_node_to_list_start(page *newNode, uint8_t order);
	void split_block(page *pageHeader, uint8_t order, size_t splits);
	void mark_block_used(page *pageHeader, uint8_t order);
	void mark_block_unused(page *pageHeader, uint8_t order);

	private:
	inline size_t get_block_size_by_order(uint8_t order);
	inline int num_of_page_headers_to_next(size_t order);
	inline page *get_page_header_by_address(void *address);

private:
	/*
	 * @brief Array of pointers to page headers for each order of memory block.
	 */
	page *_orders[BUDDY_ORDERS] = {nullptr};

public:
	/*
	 * @brief Pointer to the start of the block info array.
	 */
	void *_m_p_block_info_array_start;

	/*
	 * @brief Size of the block info array.
	 */
	size_t _m_ui_block_info_array_size;
};