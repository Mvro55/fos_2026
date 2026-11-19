#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#include "../conc/ksemaphore.h"
#include <kern/cpu/cpu.h>

/************************************************************************************/
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [0] GIVEN FUNCTIONS
//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		buddy_init();
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = KERNEL_HEAP_START;
		kheapPageAllocBreak = KERNEL_HEAP_START;
	}
	//==================================================================================
	//==================================================================================
}
/************************************************************************************/

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
/**************************************************/
/* BLOCK ALLOCATOR */
/**************************************************/
void* create_buddy_space()
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [4] create_buddy_space
	//Your code is here
	//Comment the following line
	//panic("create_buddy_space() not implemented");
	bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);
		if (!lock_already_held)
			acquire_kspinlock(&MemFrameLists.mfllock);

		uint32 va = (uint32)kmalloc(PAGE_SIZE);
		if (va == 0)
		{
			if (!lock_already_held)
				release_kspinlock(&MemFrameLists.mfllock);
			return NULL;
		}


		uint32 *page_table = NULL;
		get_page_table(ptr_page_directory, va, &page_table);
		page_table[PTX(va)] = (page_table[PTX(va)] & ~PERM_BUFFERED)
				| (PERM_AVAILABLE & ~(PERM_BUFFERED | PERM_UHPAGE));

		struct BlockMetaData *right_buddy =
			(struct BlockMetaData *)(va + BLOCK_SIZE(BUDDY_UPPER_LEVEL));
		right_buddy->level = BUDDY_UPPER_LEVEL;
		right_buddy->is_free = 1;
		LIST_INSERT_HEAD(&freeBlockLists[BUDDY_UPPER_LEVEL], right_buddy);

		if (!lock_already_held)
			release_kspinlock(&MemFrameLists.mfllock);
		return (void *)va;


}

void  delete_buddy_space(void *va)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [5] delete_buddy_space
	//Your code is here
	//Comment the following line
	//panic("delete_buddy_space() not implemented");
	uint32 addr = (uint32)va;
		bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);
		if (!lock_already_held)
			acquire_kspinlock(&MemFrameLists.mfllock);

		uint32 *page_table = NULL;
		if (addr < KERNEL_HEAP_START || addr >= kheapPageAllocBreak || (addr % PAGE_SIZE) != 0)
		{
			if (!lock_already_held)
				release_kspinlock(&MemFrameLists.mfllock);
			panic("delete_buddy_space(): invalid address %x", addr);
		}
		get_page_table(ptr_page_directory, addr, &page_table);
		if (page_table == NULL || (page_table[PTX(addr)] & PERM_PRESENT) == 0 ||
			(page_table[PTX(addr)] & (PERM_AVAILABLE & ~(PERM_BUFFERED | PERM_UHPAGE))) == 0)
		{
			if (!lock_already_held)
				release_kspinlock(&MemFrameLists.mfllock);
			panic("delete_buddy_space(): %x is not a buddy system page", addr);
		}


		page_table[PTX(addr)] = (page_table[PTX(addr)] & ~(PERM_AVAILABLE)) | PERM_BUFFERED;
		kfree(va);

		if (!lock_already_held)
			release_kspinlock(&MemFrameLists.mfllock);
}

/**************************************************/
/* PAGE ALLOCATOR */
/**************************************************/
void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [6] kmalloc
	//Your code is here
	//Comment the following line
	//kpanic_into_prompt("kmalloc() is not implemented yet...!!");
	if (size == 0)
			return NULL;

		bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);
		if (!lock_already_held)
			acquire_kspinlock(&MemFrameLists.mfllock);

		void *result = NULL;


		if (size <= DYN_ALLOC_MAX_BLOCK_SIZE - META_DATA_SIZE)
		{
			result = alloc_block(size);
		}

		else if (size <= KERNEL_HEAP_MAX - KERNEL_HEAP_START)
		{
			uint32 need = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
			uint32 breakIdx = (kheapPageAllocBreak - KERNEL_HEAP_START) / PAGE_SIZE;
			uint32 *tbl = NULL;

			uint32 cur = (uint32)frames_info[0].proc;	//index+1 of first hole (0 = none)
			uint32 prv = 0;
			uint32 exactIdx = 0, exactPrv = 0, worstIdx = 0, worstPrv = 0, worstLen = 0;
			int haveExact = 0;
			while (cur != 0)
			{
				uint32 hi = cur - 1;
				uint32 hva = KERNEL_HEAP_START + hi * PAGE_SIZE;
				get_page_table(ptr_page_directory, hva, &tbl);
				uint32 e0 = tbl[PTX(hva)];
				uint32 hlen = 1;
				if ((e0 & PERM_BUFFERED) == 0)
				{
					get_page_table(ptr_page_directory, hva + PAGE_SIZE, &tbl);
					hlen = tbl[PTX(hva + PAGE_SIZE)] >> 12;
				}
				if (hlen == need && (!haveExact || hi < exactIdx))
				{
					haveExact = 1; exactIdx = hi; exactPrv = prv;
				}
				if (hlen > worstLen || (hlen == worstLen && hi < worstIdx))
				{
					worstLen = hlen; worstIdx = hi; worstPrv = prv;
				}
				prv = cur;
				cur = e0 >> 12;
			}


			int found = 0, fromHole = 0;
			uint32 chosenIdx = 0, chosenPrv = 0, chosenLen = 0;
			if (haveExact)
			{
				found = 1; fromHole = 1; chosenIdx = exactIdx; chosenPrv = exactPrv; chosenLen = need;
			}
			else if (worstLen > need)
			{
				found = 1; fromHole = 1; chosenIdx = worstIdx; chosenPrv = worstPrv; chosenLen = worstLen;
			}
			else if (breakIdx + need <= NUM_OF_KHEAP_PAGES)
			{
				found = 1; fromHole = 0; chosenIdx = breakIdx;
			}


			if (found && LIST_SIZE(&MemFrameLists.free_frame_list) < need)
				found = 0;

			if (found)
			{

				if (fromHole)
				{
					uint32 hva = KERNEL_HEAP_START + chosenIdx * PAGE_SIZE;
					get_page_table(ptr_page_directory, hva, &tbl);
					uint32 nxt = tbl[PTX(hva)] >> 12;
					uint32 link = nxt;
					if (chosenLen > need)
					{

						uint32 ri = chosenIdx + need;
						uint32 rlen = chosenLen - need;
						uint32 rva = KERNEL_HEAP_START + ri * PAGE_SIZE;
						get_page_table(ptr_page_directory, rva, &tbl);
						tbl[PTX(rva)] = (nxt << 12) | (rlen == 1 ? PERM_BUFFERED : 0);
						if (rlen >= 2)
						{
							get_page_table(ptr_page_directory, rva + PAGE_SIZE, &tbl);
							tbl[PTX(rva + PAGE_SIZE)] = rlen << 12;
						}
						link = ri + 1;
					}
					if (chosenPrv == 0)
					{
						frames_info[0].proc = (struct Env *)link;
					}
					else
					{
						uint32 pva = KERNEL_HEAP_START + (chosenPrv - 1) * PAGE_SIZE;
						get_page_table(ptr_page_directory, pva, &tbl);
						tbl[PTX(pva)] = (tbl[PTX(pva)] & 0xFFF) | (link << 12);
					}
				}


				for (uint32 i = 0; i < need; i++)
				{
					uint32 va = KERNEL_HEAP_START + (chosenIdx + i) * PAGE_SIZE;
					get_page_table(ptr_page_directory, va, &tbl);
					tbl[PTX(va)] = 0;
					struct FrameInfo *fi = NULL;
					allocate_frame(&fi);
					map_frame(ptr_page_directory, fi, va, PERM_WRITEABLE);
					fi->proc = (struct Env *)va;
					if (i == need - 1)
						tbl[PTX(va)] |= PERM_BUFFERED;
				}

				if (!fromHole)
					kheapPageAllocBreak = KERNEL_HEAP_START + (chosenIdx + need) * PAGE_SIZE;

				result = (void *)(KERNEL_HEAP_START + chosenIdx * PAGE_SIZE);
			}
		}

		if (!lock_already_held)
			release_kspinlock(&MemFrameLists.mfllock);
		return result;
	/*EFFICIENT IMPLEMENTATION ~O(#FREE BLOCKS + #ALLOCATED PAGES) IS REQUIRED */

}

void kfree(void* virtual_address)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [7] kfree
	//Your code is here
	//Comment the following line
	//panic("kfree() not implemented");
	uint32 va = (uint32)virtual_address;
		bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);
		if (!lock_already_held)
			acquire_kspinlock(&MemFrameLists.mfllock);

		uint32 *tbl = NULL;

		if (va < KERNEL_HEAP_START || va >= kheapPageAllocBreak)
		{
			if (!lock_already_held)
				release_kspinlock(&MemFrameLists.mfllock);
			panic("kfree(): invalid address %x", va);
		}
		get_page_table(ptr_page_directory, va, &tbl);
		if (tbl == NULL || (tbl[PTX(va)] & PERM_PRESENT) == 0)
		{
			if (!lock_already_held)
				release_kspinlock(&MemFrameLists.mfllock);
			panic("kfree(): address %x is not allocated", va);
		}

		if ((tbl[PTX(va)] & (PERM_AVAILABLE & ~(PERM_BUFFERED | PERM_UHPAGE))) != 0)
		{
			free_block(virtual_address);	//may call delete_buddy_space() if the page becomes empty
			if (!lock_already_held)
				release_kspinlock(&MemFrameLists.mfllock);
			return;
		}


		if ((va % PAGE_SIZE) != 0)
		{
			if (!lock_already_held)
				release_kspinlock(&MemFrameLists.mfllock);
			panic("kfree(): address %x is not the start of an allocation", va);
		}
		if (va > KERNEL_HEAP_START)
		{
			uint32 pva = va - PAGE_SIZE;
			get_page_table(ptr_page_directory, pva, &tbl);
			uint32 pe = tbl[PTX(pva)];
			if ((pe & PERM_PRESENT) && (pe & PERM_BUFFERED) == 0 &&
				(pe & (PERM_AVAILABLE & ~(PERM_BUFFERED | PERM_UHPAGE))) == 0)
			{
				if (!lock_already_held)
					release_kspinlock(&MemFrameLists.mfllock);
				panic("kfree(): address %x is in the middle of an allocation", va);
			}
		}

		uint32 s = (va - KERNEL_HEAP_START) / PAGE_SIZE;
		uint32 n = 0;
		uint32 cva = va;
		while (1)
		{
			get_page_table(ptr_page_directory, cva, &tbl);
			uint32 ent = tbl[PTX(cva)];
			if ((ent & PERM_PRESENT) == 0 || (ent & (PERM_AVAILABLE & ~(PERM_BUFFERED | PERM_UHPAGE))) != 0)
			{
				if (!lock_already_held)
					release_kspinlock(&MemFrameLists.mfllock);
				panic("kfree(): corrupted heap while freeing %x", va);
			}
			int isLast = (ent & PERM_BUFFERED) != 0;
			unmap_frame(ptr_page_directory, cva);
			tbl[PTX(cva)] = 0;
			n++;
			if (isLast)
				break;
			cva += PAGE_SIZE;
			if (cva >= kheapPageAllocBreak)
			{
				if (!lock_already_held)
					release_kspinlock(&MemFrameLists.mfllock);
				panic("kfree(): allocation of %x has no end marker", va);
			}
		}


		uint32 ms = s, me = s + n;	//merged area [ms, me)
		for (int pass = 0; pass < 2; pass++)
		{
			uint32 cur = (uint32)frames_info[0].proc;
			uint32 prv = 0;
			while (cur != 0)
			{
				uint32 hi = cur - 1;
				uint32 hva = KERNEL_HEAP_START + hi * PAGE_SIZE;
				get_page_table(ptr_page_directory, hva, &tbl);
				uint32 e0 = tbl[PTX(hva)];
				uint32 hlen = 1;
				if ((e0 & PERM_BUFFERED) == 0)
				{
					get_page_table(ptr_page_directory, hva + PAGE_SIZE, &tbl);
					hlen = tbl[PTX(hva + PAGE_SIZE)] >> 12;
				}
				uint32 nxt = e0 >> 12;
				int match = (pass == 0) ? (hi + hlen == s) : (hi == s + n);
				if (match)
				{

					if (prv == 0)
					{
						frames_info[0].proc = (struct Env *)nxt;
					}
					else
					{
						uint32 pva = KERNEL_HEAP_START + (prv - 1) * PAGE_SIZE;
						get_page_table(ptr_page_directory, pva, &tbl);
						tbl[PTX(pva)] = (tbl[PTX(pva)] & 0xFFF) | (nxt << 12);
					}
					if (hlen >= 2)
					{
						get_page_table(ptr_page_directory, hva + PAGE_SIZE, &tbl);
						tbl[PTX(hva + PAGE_SIZE)] = 0;
					}
					get_page_table(ptr_page_directory, hva, &tbl);
					tbl[PTX(hva)] = 0;
					if (pass == 0)
						ms = hi;
					else
						me = hi + hlen;
					break;
				}
				prv = cur;
				cur = nxt;
			}
		}

		uint32 breakIdx = (kheapPageAllocBreak - KERNEL_HEAP_START) / PAGE_SIZE;
		if (me == breakIdx)
		{
			kheapPageAllocBreak = KERNEL_HEAP_START + ms * PAGE_SIZE;
		}
		else
		{
			uint32 len = me - ms;
			uint32 head = (uint32)frames_info[0].proc;
			uint32 mva = KERNEL_HEAP_START + ms * PAGE_SIZE;
			get_page_table(ptr_page_directory, mva, &tbl);
			tbl[PTX(mva)] = (head << 12) | (len == 1 ? PERM_BUFFERED : 0);
			if (len >= 2)
			{
				get_page_table(ptr_page_directory, mva + PAGE_SIZE, &tbl);
				tbl[PTX(mva + PAGE_SIZE)] = len << 12;
			}
			frames_info[0].proc = (struct Env *)(ms + 1);
		}

		if (!lock_already_held)
			release_kspinlock(&MemFrameLists.mfllock);
	/*EFFICIENT IMPLEMENTATION ~O(#FREE BLOCKS + #FREED PAGES) IS REQUIRED */

}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [8] kheap_physical_address
	//Your code is here
	//Comment the following line
	//panic("kheap_physical_address() is not implemented yet...!!");
	if (virtual_address < KERNEL_HEAP_START || virtual_address >= KERNEL_HEAP_MAX)
			return 0;

		bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);
		if (!lock_already_held)
			acquire_kspinlock(&MemFrameLists.mfllock);

		unsigned int result = 0;
		uint32 *tbl = NULL;
		get_page_table(ptr_page_directory, virtual_address, &tbl);
		if (tbl != NULL)
		{
			uint32 entry = tbl[PTX(virtual_address)];
			if ((entry & PERM_PRESENT) == PERM_PRESENT)
				result = EXTRACT_ADDRESS(entry) + PGOFF(virtual_address);
		}

		if (!lock_already_held)
			release_kspinlock(&MemFrameLists.mfllock);
		return result;
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'26.GM#2] KERNEL HEAP - [9] kheap_virtual_address
	//Your code is here
	//Comment the following line
	//panic("kheap_virtual_address() is not implemented yet...!!");
	if (PPN(physical_address) >= number_of_frames)
			return 0;

		bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);
		if (!lock_already_held)
			acquire_kspinlock(&MemFrameLists.mfllock);

		unsigned int result = 0;

		uint32 v = (uint32)frames_info[PPN(physical_address)].proc;
		if (v >= KERNEL_HEAP_START && v < KERNEL_HEAP_MAX && (v % PAGE_SIZE) == 0)
		{
			uint32 *tbl = NULL;
			get_page_table(ptr_page_directory, v, &tbl);
			if (tbl != NULL)
			{
				uint32 entry = tbl[PTX(v)];
				if ((entry & PERM_PRESENT) == PERM_PRESENT &&
					EXTRACT_ADDRESS(entry) == EXTRACT_ADDRESS(physical_address))
					result = v + PGOFF(physical_address);
			}
		}

		if (!lock_already_held)
			release_kspinlock(&MemFrameLists.mfllock);
		return result;

	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().
void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'26.BONUS#1] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
