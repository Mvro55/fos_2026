// Sleeping locks

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "sleeplock.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

/************************************************************************************/
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//TODO: [PROJECT'26.GM#4] BLOCKING - [0] GIVEN FUNCTIONS

void init_sleeplock(struct sleeplock *lk, char *name)
{
	init_channel(&(lk->chan), "sleep lock channel");
	char prefix[30] = "lock of sleeplock - ";
	char guardName[30+NAMELEN];
	strcconcat(prefix, name, guardName);
	init_kspinlock(&(lk->lk), guardName);
	strcpy(lk->name, name);
	lk->locked = 0;
	lk->pid = 0;
}

int holding_sleeplock(struct sleeplock *lk)
{
	int r;
	acquire_kspinlock(&(lk->lk));
	r = lk->locked && (lk->pid == get_cpu_proc()->env_id);
	release_kspinlock(&(lk->lk));
	return r;
}
/************************************************************************************/

//=====================================================================================//
//============================== REQUIRED FUNCTIONS ===================================//
//=====================================================================================//
void acquire_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'26.GM#4] BLOCKING - [16] SleepLock acquire_sleeplock
	acquire_kspinlock(&(lk->lk));
	{
		// "while" (not "if"): release wakes ALL waiters, but only one can take
		// the lock; the others must re-check and go back to sleep.
		while (lk->locked)
		{
			sleep(&(lk->chan), &(lk->lk));	// releases lk->lk while asleep, reacquires on wakeup
		}
		lk->locked = 1;
		struct Env *p = get_cpu_proc();
		lk->pid = (p != NULL) ? p->env_id : 0;
	}
	release_kspinlock(&(lk->lk));
}

void release_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'26.GM#4] BLOCKING - [17] SleepLock release_sleeplock
	acquire_kspinlock(&(lk->lk));
	{
		lk->locked = 0;
		lk->pid = 0;
		wakeup_all(&(lk->chan));
	}
	release_kspinlock(&(lk->lk));
}




