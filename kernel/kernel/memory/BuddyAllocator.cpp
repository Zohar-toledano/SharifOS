#include <kernel/memory/MemoryManager.h>
#include <kernel/memory/BuddyAllocator.h>
#include <string.h>
#include <kernel/kernel.h>

inline void *page::getBlockStart()
{
	return (void *)(((size_t)this - (size_t)krn.memoryManager.buddyAllocator._m_p_block_info_array_start) / sizeof(page) * BUDDY_ALLOCATOR_MIN_BLOCK_SIZE);
}
inline bool page::isUsed()
{
	return (flags & 0x01);
}

inline void page::setUnused()
{
	flags &= 0xFE;
}
inline void page::setUsed()
{
	flags |= 0x01;
}

inline void page::setChained() { flags |= 0x02; } // 1 << 1
inline void page::clearChained() { flags &= ~0x02; }
inline bool page::isChained() const { return flags & 0x02; }

/**
 * Initializes the BuddyAllocator.
 *
 * This function is responsible for initializing the BuddyAllocator's state, including
 * the block info array and the linked lists for each order.
 *
 * It also reserves memory for the kernel within the block info array.
 *
 * @note This function is called by the MemoryManager during its initialization.
 */
void BuddyAllocator::init()
{
	// Calculate the starting address for the block info array, ensuring 4K alignment
	_m_p_block_info_array_start = (void *)(ALIGN_UP_4K(
		((size_t)(krn.memoryManager.p_kernel_start) + krn.memoryManager.ul_kernel_size)));

	// Calculate the size of the block info array, ensuring 4K alignment
	_m_ui_block_info_array_size = (ALIGN_UP_4K((krn.memoryManager.ul_physical_memory_size / BUDDY_ALLOCATOR_MIN_BLOCK_SIZE) * sizeof(page)));

	// Initialize the memory block info array to zero
	memset((void *)_m_p_block_info_array_start, 0, _m_ui_block_info_array_size); // Should be at MemoryManager

	// TODO: Handle any leftover memory that doesn't fit neatly into a block
	// Find the closest lower order for the total physical memory size
	uint8_t order = find_closest_lower_order(krn.memoryManager.ul_physical_memory_size);
	size_t start_addr = (size_t)krn.memoryManager.ul_memory_start;
	page *base;
	int offset = 0;

	// Iterate over each order from the highest down to 1
	for (order; order > 0; order--)
	{
		// Calculate the number of page headers needed to move to the next order
		int step = num_of_page_headers_to_next(order);
		bool first = true;
		// Determine the size of each block at the current order
		int page_size = get_block_size_by_order(order);

		// Continue adding pages until the start address exceeds the available memory
		while (true)
		{
			if (start_addr + page_size > (size_t)krn.memoryManager.ul_memory_start + krn.memoryManager.ul_physical_memory_size)
			{
				break;
			}

			// Calculate the base page at the current offset
			base = ((page *)_m_p_block_info_array_start) + offset;
			page *prev = nullptr;

			// If this is not the first page, link the previous page to the current
			if (!first)
			{
				prev = base - step;
				prev->lru.next = (void *)base;
			}
			else
			{
				// Otherwise, set this page as the start of the order list
				_orders[order] = base;
				first = false;
			}

			// Link the previous page to the current page
			base->lru.prev = (void *)prev;
			// Mark the current block as chained
			base->setChained();

			// Move the start address forward by the block size
			start_addr += page_size;
			// Move the offset forward by the step size
			offset += step;
		}
	}

	// // Reserve memory for the kernel within the block info array
	{
		void *start = (void *)(krn.memoryManager.p_kernel_start);
		void *end = (void *)((size_t)_m_p_block_info_array_start + _m_ui_block_info_array_size);
		printf("start reserved: %x\nend reserved: %x\n", start, end);
		printf("stack %x\n", &start);
		reserved_memory(start, end);
	}

	// // Placeholder: Reserve memory for the stack
	// {
	// 	// stack
	// }

	// // Placeholder: Reserve memory for VGA
	// {
	// 	// vga
	// }
	// void *SharifNullPageHaram = allocate(0,(void*)0x0);

	// page *p = get_page_header_by_address((void *)(krn.memoryManager.p_kernel_start));
	// printf("page free status: %d, order: %d,next: %x,prev: %x\n", p->flags, p->order, p->lru.next, p->lru.prev);
	// printf("start reserved: %x\nend reserved: %x\n", krn.memoryManager.p_kernel_start, m_p_block_info_array_start + _m_ui_block_info_array_size);
	// for (size_t o = 0; o < BUDDY_ORDERS; o++)
	// {
	// 	printf("BuddyAllocator::init: _orders[%d] = 0x%d\n", o, get_free_blocks_len(o));
	// }

	// void *A = allocate(0x100);
	// void *B = allocate(0x100);
	// void *C = allocate(1, (void *)0x4000);
	// printf("C: %x\n", C);
	// printf("A: %x\nB: %x\n", A, B);
	// // free(A);
	// free(B);
	// A = allocate(0x100);
	// B = allocate(0x100);
	// printf("A: %x\nB: %x\n", A, B);
}

/**
 * Frees a block of memory allocated by the BuddyAllocator.
 *
 * This function will free a block of memory allocated by the BuddyAllocator.
 * It takes the address of the block as a parameter and will return true if the
 * block was successfully freed, or false if the address is invalid or the block
 * is not allocated.
 *
 * @param address The address of the block to free.
 * @return true if the block was successfully freed, false otherwise.
 */
bool BuddyAllocator::free(void *address)
{
	// Check if the provided address is null
	if (address == nullptr)
	{
		// Return false as there is nothing to free
		return false;
	}

	// Align the address to the nearest 4K boundary
	size_t aligned_address = ALIGN_UP_4K((size_t)address);
	// Check if the aligned address is different from the original address
	if (aligned_address != (size_t)address)
	{
		// Return false as the address is not 4K aligned
		return false;
	}

	// Retrieve the page header corresponding to the address
	page *header = get_page_header_by_address(address);

	// Verify if the address is within the bounds of block_info_array
	if ((size_t)header < (size_t)_m_p_block_info_array_start || (size_t)header >= (size_t)_m_p_block_info_array_start + _m_ui_block_info_array_size)
	{
		// Return false if the address is out of bounds
		return false;
	}

	// Check if the block is currently marked as used
	if (!header->isUsed())
	{
		// Return false if it's already free
		return false;
	}

	// Retrieve the order of the block
	uint8_t order = header->order;
	// Mark the block as unused in the buddy system
	mark_block_unused(header, order);

	// Return true indicating the block was successfully freed
	return true;
}

/**
 * Allocates a block of memory with the specified size.
 *
 * This function will allocate a block of memory with the specified size.
 * The function will find the order of the block that is closest to the size
 * requested and allocate a block of that order. If there are no free blocks
 * of the desired order, the function will split larger blocks to satisfy the
 * request.
 *
 * @param size The size of the block to be allocated
 * @return A pointer to the allocated block of memory or nullptr if the
 * allocation failed.
 */
void *BuddyAllocator::allocate(size_t size)
{
	// Find the order of the block that is closest to the size requested
	uint8_t order = find_closest_upper_order(size);

	// Initialize a counter to keep track of the number of blocks to split
	size_t count = 0;

	// Iterate over the _orders from the closest order to the largest order
	for (order; order < BUDDY_ORDERS; order++)
	{
		// Get the header of the block for the current order
		page *header = _orders[order];

		// Check if the header is valid
		if (header != nullptr)
		{
			split_block(header, order, count);
			// Mark the block as used in the buddy system
			mark_block_used(header, order);

			// Return the address of the block
			return header->getBlockStart();
		}

		// Increment the counter for the number of blocks to split
		count++;
	}

	// If we reach here, it means we couldn't find a suitable block
	// Return nullptr to indicate failure
	return nullptr; // No suitable block found
}

/**
 * Allocates a block of memory with the specified order.
 *
 * This function will allocate a block of memory with the specified order.
 * The preferred address is the address of the block that should be allocated.
 * If the preferred address is nullptr, the function will allocate a block
 * of the desired order.
 *
 * @param desiredOrder The desired order of the block.
 * @param preferredAddr The preferred address of the block.
 * @return The address of the allocated block, or nullptr if the allocation failed.
 */
void *BuddyAllocator::allocate(uint8_t desiredOrder, void *preferredAddr)
{
	// If the preferred address is nullptr, just allocate a block of the desired order
	if (preferredAddr == nullptr)
	{
		return allocate(get_block_size_by_order(desiredOrder));
	}

	// Check if the preferred address is aligned to the block size of the desired order
	if (!IS_ALIGNED(preferredAddr, get_block_size_by_order(desiredOrder)))
	{
		// If not, return nullptr to indicate failure
		return nullptr;
	}

	// Get the page header of the preferred address
	page *preferredPage = get_page_header_by_address(preferredAddr);

	// Get the page header of the first free block in the list
	page *freeListPage = preferredPage;

	// Traverse the list back to the first free block
	while (!freeListPage->isChained())
	{
		freeListPage--;
	}

	// If the first free block is used, return nullptr to indicate failure
	if (freeListPage->isUsed())
	{
		return nullptr;
	}
	// Find the order of the first free block
	uint8_t order = 255;
	page *orderPage = freeListPage;

	// Traverse the list to find the order of the first free block
	while (orderPage->lru.prev != nullptr)
	{
		orderPage = (page *)orderPage->lru.prev;
	}

	// Find the order of the first free block
	for (int i = 0; i < BUDDY_ORDERS; i++)
	{
		// printf("_orders[%d]: 0x%x\n", i, _orders[i]);
		if (orderPage == _orders[i])
		{
			order = i;
			break;
		}
	}
	// If the order is invalid, return nullptr to indicate failure
	if (order == 255 || desiredOrder > order)
	{
		return nullptr;
	}
	// Split the block until the desired order is reached
	remove_node_from_list(freeListPage, order);
	while (desiredOrder < order && order < BUDDY_ORDERS)
	{
		// if (order == 0) break;
		// Remove the block from the free list

		// Decrement the order
		order--;

		// Calculate the hop to the next block in the list
		size_t hop = num_of_page_headers_to_next(order);

		// Split the block into two buddies
		page *buddyA = freeListPage;
		page *buddyB = freeListPage + hop;

		// Check if the preferred address is within the range of the first buddy
		if (preferredPage < buddyB && preferredPage >= buddyA)
		{
			// Add the second buddy to the free list
			add_node_to_list_start(buddyB, order);

			// Move the first buddy to the next block in the list
			freeListPage = buddyA;
		}
		// Check if the preferred address is within the range of the second buddy
		else if (preferredPage >= buddyB && preferredPage < buddyB + hop)
		{
			// Add the first buddy to the free list
			add_node_to_list_start(buddyA, order);

			// Move the second buddy to the next block in the list
			freeListPage = buddyB;
		}
		// If the preferred address is not within the range of either buddy, return nullptr to indicate failure
		else
		{
			return nullptr;
		}
	}

	// Mark the block as used in the buddy system
	mark_block_used(freeListPage, order);
	// Return the address of the block
	return freeListPage->getBlockStart();
}

/**
 * Marks a block as used in the buddy system.
 *
 * This function takes a pointer to the page header of the block and the order of the block as parameters.
 * The order is the log2 of the size of the block.
 *
 * This function is used to mark a block as used in the buddy system.
 * It takes a pointer to the page header of the block and the order of the block as parameters.
 * The order is the log2 of the size of the block.
 *
 * The function first removes the block from the free list. This is necessary because we're about to split the block into two buddies,
 * and we don't want the second buddy to be on the free list.
 *
 * The function then splits the block into two buddies. We'll do this by iterating from 0 to the desired number of splits.
 * On each iteration, we'll decrement the order of the block and add the second buddy to the free list.
 * We'll also update the 'current' pointer to point to the first buddy.
 *
 * Finally, the function marks the block as used by setting the 'used' bit to true.
 * It also sets the order of the block to the desired order.
 *
 * @param pageHeader A pointer to the page header of the block.
 * @param order The order of the block.
 * @param splits The desired number of splits.
 */
void BuddyAllocator::mark_block_used(page *pageHeader, uint8_t order)
{

	// Finally, mark the block as used by setting the 'used' bit to true.
	pageHeader->setUsed();

	// Set the order of the block to the desired order.
	pageHeader->order = order;
}

/**
 * @brief Marks a block as unused in the buddy system.
 *
 * This function takes a pointer to the page header of the block and the order of the block as parameters.
 * The order is the log2 of the size of the block.
 *
 * This function is used to mark a block as unused in the buddy system.
 * It takes a pointer to the page header of the block and the order of the block as parameters.
 * The order is the log2 of the size of the block.
 *
 * The function first checks if the block is aligned to the next order.
 * If it is, it checks if the buddy is used or not.
 * If the buddy is not used, it removes it from the free list and merges it with the current block.
 * If the buddy is used, it breaks out of the loop.
 *
 * The function then adds the block to the free list.
 * It sets the order of the block to 0.
 * It marks the block as unused.
 *
 * @param pageHeader A pointer to the page header of the block to mark as unused.
 * @param order The order of the block to mark as unused.
 */
void BuddyAllocator::mark_block_unused(page *pageHeader, uint8_t order)
{
	// We need to keep track of the order of the block as we iterate through the loop.
	uint8_t buddyOrder = order;

	// We'll loop until we reach the maximum order (which is one less than the number of _orders).
	while (buddyOrder < BUDDY_ORDERS)
	{
		// We need to check if the block is aligned to the next order.
		// If it is, we need to check if the buddy is used or not.
		// If the buddy is not used, we need to remove it from the free list and merge it with the current block.
		page *alignedBuddy;
		page *buddy;

		// Check if the block is aligned to the next order.
		if (IS_ALIGNED(pageHeader->getBlockStart(), get_block_size_by_order(buddyOrder + 1)))
		{
			// If it is, the buddy is the next block in the list.
			buddy = pageHeader + num_of_page_headers_to_next(buddyOrder);
			// The aligned buddy is the current block.
			alignedBuddy = pageHeader;
		}
		else
		{
			// If it's not, the buddy is the previous block in the list.
			buddy = pageHeader - num_of_page_headers_to_next(buddyOrder);
			// The aligned buddy is the buddy block.
			alignedBuddy = buddy;
		}

		// Check if the buddy is used.
		if (!buddy->isUsed())
		{
			// If it's not, remove it from the free list.
			remove_node_from_list(buddy, buddyOrder);
			// Move the page header to the aligned buddy.
			pageHeader = alignedBuddy;
		}
		else
		{
			// If it is, break out of the loop.
			break;
		}

		// Increment the order.
		buddyOrder++;
	}

	// Add the block to the free list.
	add_node_to_list_start(pageHeader, order);

	// Set the order of the block to 0.
	pageHeader->order = 0;

	// Mark the block as unused.
	pageHeader->setUnused();
}

/**
 * @brief Adds a node to the free list of a given order in the buddy system.
 *
 * This function takes a pointer to the page header of the block and the order of the block as parameters.
 * The order is the log2 of the size of the block.
 *
 * This function is used to add a block to the free list of the buddy system.
 * It takes a pointer to the page header of the block and the order of the block as parameters.
 * The order is the log2 of the size of the block.
 *
 * @param newNode The page header of the block to add to the free list.
 * @param order The order of the block to add to the free list.
 */
void BuddyAllocator::add_node_to_list_start(page *newNode, uint8_t order)
{
	// Get the next node in the list.
	page *nextNode = _orders[order];
	// Set the next node of the new node to the next node.
	newNode->lru.next = nextNode;
	// Set the previous node of the new node to nullptr, since it's the first node in the list.
	newNode->lru.prev = nullptr;
	// Set the previous node of the next node to the new node.
	if (nextNode != nullptr)
		nextNode->lru.prev = newNode;
	// Set the next node in the list to the new node.
	_orders[order] = newNode;
	// Set the "chained" bit of the new node to true, since it's in the list.
	newNode->setChained();
}

void BuddyAllocator::split_block(page *pageHeader, uint8_t order, size_t splits)
{
	// Remove the block from the free list. This is necessary because we're about to split the block into two buddies,
	// and we don't want the second buddy to be on the free list.
	remove_node_from_list(pageHeader, order);

	// We're going to be splitting the block into two buddies, and we want to keep track of which buddy is which.
	// We'll use a pointer called 'current' to keep track of which buddy we're currently working with.
	page *current = pageHeader;

	// Split the block into two buddies. We'll do this by iterating from 0 to the desired number of splits.
	// On each iteration, we'll decrement the order of the block and add the second buddy to the free list.
	// We'll also update the 'current' pointer to point to the first buddy.
	for (size_t splitCount = 0; splitCount < splits; ++splitCount)
	{
		--order;
		page *buddyA = current;									// The first buddy is the one we're currently working with.
		page *buddyB = current + num_of_page_headers_to_next(order); // The second buddy is the one that we'll add to the free list.
		add_node_to_list_start(buddyB, order);						// Add the second buddy to the free list.
		current = buddyA;										// Update the 'current' pointer to point to the first buddy.
	}
}

/**
 * @brief Removes a node from the free list of a given order in the buddy system.
 *
 * This function takes a pointer to the page header of the block and the order of the block as parameters.
 * The order is the log2 of the size of the block.
 *
 * This function is used to remove a block from the free list of the buddy system.
 * It takes a pointer to the page header of the block and the order of the block as parameters.
 * The order is the log2 of the size of the block.
 *
 * The function first checks if the node is the only node in the list by checking if both the next and previous nodes are null.
 * If it is, the function sets the head of the list for the given order to null as the list is now empty.
 *
 * If the node is not the only node in the list, the function then updates the previous node's next pointer to bypass the current node.
 * It also updates the next node's previous pointer to bypass the current node.
 *
 * Finally, the function clears the next and previous pointers of the current node and marks the node as no longer part of the list using the clearChained method.
 *
 * @param node A pointer to the page header of the block.
 * @param order The order of the block.
 */
void BuddyAllocator::remove_node_from_list(page *node, uint8_t order)
{
	// Retrieve the next node in the list from the current node's lru.next pointer
	page *nextNode = static_cast<page *>(node->lru.next);
	// Retrieve the previous node in the list from the current node's lru.prev pointer
	page *prevNode = static_cast<page *>(node->lru.prev);

	// Check if both nextNode and prevNode are null, indicating the node is the only node in the list
	if (nextNode == nullptr && prevNode == nullptr)
		// Set the head of the list for the given order to null as the list is now empty
		_orders[order] = nullptr;
	else
	{
		// If prevNode is null, it means the node is the head of the list
		if (prevNode == nullptr)
			// Update the head of the list to point to the nextNode
			_orders[order] = nextNode;
		else
			// Otherwise, update the previous node's next pointer to bypass the current node
			prevNode->lru.next = nextNode;

		// If nextNode is not null, update its previous pointer to bypass the current node
		if (nextNode != nullptr)
			nextNode->lru.prev = prevNode;
	}

	// Clear the next and previous pointers of the current node
	node->lru.next = nullptr;
	node->lru.prev = nullptr;
	// Mark the node as no longer part of the list using the clearChained method
	node->clearChained();
}

/**
 * @brief Get the number of free blocks of a given order in the buddy system.
 *
 * This function traverses the linked list of free blocks for the specified order
 * and counts the total number of free blocks available.
 *
 * @param order The order of the blocks to count.
 * @return The total number of free blocks of the given order.
 */
size_t BuddyAllocator::get_free_blocks_len(uint8_t order)
{
	// Initialize the count of free blocks to zero
	int count = 0;

	// Start with the head of the linked list for the given order
	page *curr = _orders[order];

	// Traverse the linked list to count the number of free blocks
	while (curr != nullptr)
	{
		// Increment the count for each block encountered
		++count;

		// Move to the next block in the list by following the lru.next pointer
		curr = reinterpret_cast<page *>(curr->lru.next);
	}

	// Return the total number of free blocks found
	return count;
}

/**
 * @brief Retrieve the block size corresponding to the given order.
 *
 * This function returns the size of a block in the buddy system for the given order.
 * The block size is calculated by left-shifting the base block size by the given order.
 *
 * @param order The order of the blocks to retrieve the size for.
 * @return The size of the block in the buddy system for the given order.
 */
size_t BuddyAllocator::get_block_size_by_order(uint8_t order)
{
	// Calculate the block size by left-shifting the base block size by the given order
	return BUDDY_ALLOCATOR_MIN_BLOCK_SIZE << order;
}

void *test(uint8_t t, void *end)
{
	printf("hiiiiiiiiiiiiiiiiiii\n");

	return nullptr;
}

/**
 * @brief Reserve memory by allocating all blocks in the range of memory.
 *
 * @param start The start address of the range of memory to reserve.
 * @param end The end address of the range of memory to reserve.
 */
void BuddyAllocator::reserved_memory(void *start, void *end)
{
	void *current = start;

	// Iterate through the range of memory by incrementing the current pointer
	// by the minimum block size of the buddy allocator
	while (current < end)
	{
		// Attempt to allocate a block of memory of the lowest order (0)
		// at the current address
		// test(0,current);
		if (allocate(0, current) == nullptr)
		{
			printf("error %x\n", current);
			break;
		}
		// // Move the current pointer to the next block of memory
		current = (void *)((size_t)current + BUDDY_ALLOCATOR_MIN_BLOCK_SIZE);
	}
}

/**
 * @brief Calculates the number of page headers to the next block of memory.
 *
 * @param order The order of the block to calculate the number of page headers for.
 * @return The number of page headers to the next block of memory.
 */
inline int BuddyAllocator::num_of_page_headers_to_next(size_t order)
{
	// Calculate the number of page headers to the next block of memory
	// by left-shifting 1 by the given order
	return 1UL << order;
}

/**
 * @brief Calculates the page header for a given address.
 *
 * @param address The address to calculate the page header for.
 * @return The page header for the given address.
 */
inline page *BuddyAllocator::get_page_header_by_address(void *address)
{
	// Calculate the page header by dividing the address by the minimum block size
	// and adding the result to the starting address of the block info array
	return (page *)_m_p_block_info_array_start + ((size_t)address / BUDDY_ALLOCATOR_MIN_BLOCK_SIZE);
}

/**
 * @brief Find the closest lower order for a given size.
 *
 * This function takes a size in bytes and returns the closest lower order that
 * is less than or equal to the given size. The order is calculated by right-shifting
 * the size by 12 to get the order in terms of 4KB blocks.
 *
 * @param size The size in bytes to find the closest lower order for.
 * @return The closest lower order that is less than or equal to the given size.
 */
int BuddyAllocator::find_closest_lower_order(size_t size)
{
	// Right-shift the size by 12 to get the order in terms of 4KB blocks
	size = size >> 12;
	int order = -1;

	// Iterate over the _orders until the size is less than or equal to 0
	while (order < BUDDY_ORDERS - 1 && size > 0)
	{
		// Right-shift the size to move to the next order
		size = size >> 1;
		order++;
	}

	// Return the closest lower order
	return order;
}

/**
 * @brief Find the closest upper order for a given size.
 *
 * This function takes a size in bytes and returns the closest upper order that
 * is greater than or equal to the given size. The order is calculated by iterating
 * over the _orders until the size is less than the block size for the current order.
 *
 * @param size The size in bytes to find the closest upper order for.
 * @return The closest upper order that is greater than or equal to the given size.
 */
size_t BuddyAllocator::find_closest_upper_order(size_t size)
{
	uint8_t order = 0;
	while (order < BUDDY_ORDERS - 1 && get_block_size_by_order(order) < size)
	{
		// Increment the order to move to the next larger block size
		order++;
	}
	return order;
}
