/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <inc/string.h>
#include <inc/disk.h>

/************************************************************************************/
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//TODO: [PROJECT'26.GM#4] BLOCKING - [0] GIVEN FUNCTIONS
//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}
/************************************************************************************/

//=====================================================================================//
//============================== REQUIRED FUNCTIONS ===================================//
//=====================================================================================//
//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct kspinlock* lk)
{
	//TODO: [PROJECT'26.GM#4] BLOCKING - [13] Channel sleep
	struct Env *p = get_cpu_proc();
	if (p == NULL)
		panic("sleep: no running process to block");
	if (lk == NULL)
		panic("sleep: no lock");
	if (!holding_kspinlock(lk))
		panic("sleep: lock \"%s\" is not held by this CPU", lk->name);

	// To avoid a missed wake-up, qlock MUST be acquired BEFORE releasing lk:
	// once we hold qlock, no wakeup (which needs qlock) can scan the channel
	// until we're already in its queue and fully blocked.
	// If lk IS the qlock itself, it's already held => skip the swap.
	bool lk_is_qlock = (lk == &ProcessQueues.qlock);
	if (!lk_is_qlock)
	{
		acquire_kspinlock(&ProcessQueues.qlock);
		release_kspinlock(lk);
	}

	// Block the current process on the channel
	p->env_status = ENV_BLOCKED;
	enqueue(&(chan->queue), p);

	// Switch to the scheduler (needs: qlock held ONLY, ncli == 1).
	// We come back here only after a wakeup moved us to ready & the scheduler picked us.
	sched();

	// Reacquire the original lock (the scheduler hands qlock over to us)
	if (!lk_is_qlock)
	{
		release_kspinlock(&ProcessQueues.qlock);
		acquire_kspinlock(lk);
	}
}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'26.GM#4] BLOCKING - [14] Channel wakeup_one
	// Callers (ISRs, release_sleeplock, tests) usually do NOT hold qlock,
	// so take it only if it isn't already held by this CPU.
	bool qlock_already_held = holding_kspinlock(&ProcessQueues.qlock);
	if (!qlock_already_held)
		acquire_kspinlock(&ProcessQueues.qlock);
	{
		struct Env *e = dequeue(&(chan->queue));
		if (e != NULL)
			sched_insert_ready(e);	// sets ENV_READY & inserts in the proper ready queue
	}
	if (!qlock_already_held)
		release_kspinlock(&ProcessQueues.qlock);
}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'26.GM#4] BLOCKING - [15] Channel wakeup_all
	bool qlock_already_held = holding_kspinlock(&ProcessQueues.qlock);
	if (!qlock_already_held)
		acquire_kspinlock(&ProcessQueues.qlock);
	{
		struct Env *e;
		while ((e = dequeue(&(chan->queue))) != NULL)
			sched_insert_ready(e);
	}
	if (!qlock_already_held)
		release_kspinlock(&ProcessQueues.qlock);
}

