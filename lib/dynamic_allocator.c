/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

/************************************************************************************/
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//TODO: [PROJECT'26.GM#1] DYNAMIC ALLOCATOR - [0] GIVEN FUNCTIONS
//==================================
// [1] INITIALIZE BUDDY FREE LISTS:
//==================================
void buddy_init()
{
	//panic("buddy_init is not implemented yet");

	for (int l = BUDDY_LOWER_LEVEL; l <= BUDDY_UPPER_LEVEL; ++l)
	{
		LIST_INIT(&(freeBlockLists[l]));
	}
}
//====================================
// [2] PRINT SIZE OF FREE LISTS:
//====================================
void PrintFreeBlockLists()
{
	for (int i = BUDDY_LOWER_LEVEL; i <= BUDDY_UPPER_LEVEL; ++i)
	{
		cprintf("Level#%d: %d\n", i, LIST_SIZE(&freeBlockLists[i]));
	}
}
/************************************************************************************/

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===========================
// [1] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'26.GM#1] DYNAMIC ALLOCATOR - [1] get_block_size
	//Your code is here
	//Comment the following line
	if (va == NULL)
			return 0;
		struct BlockMetaData *meta = (struct BlockMetaData *)((char *)va - META_DATA_SIZE);
		return BLOCK_SIZE(meta->level);
	//panic("get_block_size() not implemented");

}
//===========================
// [2] ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 requiredSize)
{
	//TODO: [PROJECT'26.GM#1] DYNAMIC ALLOCATOR - [2] alloc_block
	//Your code is here
	//Comment the following line
	if (requiredSize == 0)
			return NULL;

		uint32 total_size = requiredSize + META_DATA_SIZE;
		int target_level = BUDDY_LOWER_LEVEL;

		while ((1 << target_level) < total_size)
		{
			target_level++;
		}

		if (target_level > BUDDY_UPPER_LEVEL)
			return NULL;

		int current_level = target_level;

		while (current_level <= BUDDY_UPPER_LEVEL && LIST_FIRST(&freeBlockLists[current_level]) == NULL)
		{
			current_level++;
		}

		if (current_level > BUDDY_UPPER_LEVEL)
		{
			void *new_page = create_buddy_space();
			if (new_page == NULL)
				return NULL;

			struct BlockMetaData *page_meta = (struct BlockMetaData *)new_page;
			page_meta->level = BUDDY_UPPER_LEVEL;
			page_meta->is_free = 1;

			LIST_INSERT_HEAD(&(freeBlockLists[BUDDY_UPPER_LEVEL]), page_meta);
			current_level = BUDDY_UPPER_LEVEL;
		}

		struct BlockMetaData *block = LIST_FIRST(&freeBlockLists[current_level]);
		LIST_REMOVE(&(freeBlockLists[current_level]), block);

		while (current_level > target_level)
		{
			current_level--;

			uint32 block_va = (uint32)block;
			uint32 buddy_va = block_va + BLOCK_SIZE(current_level);
			struct BlockMetaData *buddy = (struct BlockMetaData *)buddy_va;

			block->level = current_level;
			block->is_free = 1;

			buddy->level = current_level;
			buddy->is_free = 1;

			LIST_INSERT_HEAD(&(freeBlockLists[current_level]), buddy);
		}

		block->level = target_level;
		block->is_free = 0;

		return (void *)((char *)block + META_DATA_SIZE);
	//panic("alloc_block() not implemented");
}

//===================================================
// [3] FREE BLOCK WITH COALESCING:
//===================================================
void free_block(void *va)
{
	//TODO: [PROJECT'26.GM#1] DYNAMIC ALLOCATOR - [3] free_block
	//Your code is here
	//Comment the following line
	if (va == NULL)
			return;

		struct BlockMetaData *block = (struct BlockMetaData *)((char *)va - META_DATA_SIZE);

		if (block->is_free)
			return;

		int current_level = block->level;
		block->is_free = 1;

		uint32 block_va = (uint32)block;
		uint32 page_start_va = ROUNDDOWN(block_va, PAGE_SIZE);

		while (current_level < BUDDY_UPPER_LEVEL)
		{
			uint32 offset = block_va - page_start_va;
			uint32 buddy_offset = offset ^ BLOCK_SIZE(current_level);
			uint32 buddy_va = page_start_va + buddy_offset;

			if (buddy_va >= page_start_va + PAGE_SIZE)
			{
				break;
			}

			struct BlockMetaData *buddy = (struct BlockMetaData *)buddy_va;

			if (buddy->is_free && buddy->level == current_level)
			{
				LIST_REMOVE(&(freeBlockLists[current_level]), buddy);

				if ((uint32)buddy < (uint32)block)
				{
					block = buddy;
				}

				current_level++;
				block->level = current_level;
				block->is_free = 1;
				block_va = (uint32)block;
			}
			else
			{
				break;
			}
		}

		if (current_level == BUDDY_UPPER_LEVEL)
		{
			uint32 offset = block_va - page_start_va;
			uint32 buddy_offset = offset ^ BLOCK_SIZE(BUDDY_UPPER_LEVEL);
			uint32 buddy_va = page_start_va + buddy_offset;

			struct BlockMetaData *buddy = (struct BlockMetaData *)buddy_va;

			if (buddy->is_free && buddy->level == BUDDY_UPPER_LEVEL)
			{
				LIST_REMOVE(&(freeBlockLists[BUDDY_UPPER_LEVEL]), buddy);
				delete_buddy_space((void *)page_start_va);
			}
			else
			{
				LIST_INSERT_TAIL(&(freeBlockLists[BUDDY_UPPER_LEVEL]), block);
			}
		}
		else
		{
			LIST_INSERT_TAIL(&(freeBlockLists[current_level]), block);
		}
	//panic("free_block() not implemented");
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 newSize)
{
	//TODO: [PROJECT'26.BONUS#1] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
