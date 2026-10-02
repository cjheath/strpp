#if !defined(LOCK_H)
#define LOCK_H
/*
 * Lock and SIXLock: locks that hold a thread off for as long as you hold one.
 *
 * A Lock is exclusive: one thread at a time holds an ExclLock on it.
 *
 * A SIXLock is Shared, Intent or Exclusive:
 *	SharedLock	Any number of threads at once, none of whom may write.
 *	IntentLock	One thread. Existing SharedLocks continue, but nobody new gets
 *			any lock until this one is released. You can upgrade() it
 *			to an ExclLock, which waits for the SharedLocks to finish.
 *	ExclLock	One thread, and no others hold any lock at all.
 *
 * You hold a lock by keeping its object in scope; destroying the object
 * releases it. An object cannot be copied but you can move it. A lock with
 * a Milliseconds timeout may not be granted: holding() tells you which.
 *
 * A thread that already holds an IntentLock or ExclLock cannot wait for another
 * lock on the same Lock. It would wait for itself, so that stops the program.
 * With a timeout it just doesn't get the lock.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<lockfree.h>
#include	<condition.h>
#include	<thread.h>
#include	<datetime.h>
#include	<strassert.h>

class	SharedLock;
class	IntentLock;
class	ExclLock;

class	Lock
{
public:
	Lock() : writer(false), readers(0), owner() {}
	~Lock() { StrppAssert(!writer && readers == 0); }	// All locks must be released first

	Lock(const Lock&) = delete;
	Lock&		operator=(const Lock&) = delete;

	bool		writing()		// Does a thread hold an Intent or Exclusive lock?
			{ latch.enter(); bool w = writer; latch.leave(); return w; }
	int		sharing()		// How many SharedLocks are there?
			{ latch.enter(); int r = readers; latch.leave(); return r; }

private:
	Latch		latch;
	Condition	cond;
	bool		writer;		// An Intent or Exclusive lock is granted...
	int		readers;	// ...and this many Shared locks
	ThreadId	owner;		// ...to this thread, when writer

	friend class	SharedLock;
	friend class	IntentLock;
	friend class	ExclLock;

	// Each of these is called with the latch held. A null timeout waits as long as it takes.
	template<class Blocked>
	bool		wait_while(Blocked blocked, const Milliseconds* timeout);
	bool		take_shared(const Milliseconds* timeout);
	bool		take_intent(const Milliseconds* timeout);
	bool		drain(const Milliseconds* timeout);	// Wait for the SharedLocks to end
	void		check_not_own(const Milliseconds* timeout) const;
	void		release_shared();
	void		release_writer();
};

// Only a SIXLock grants SharedLocks and IntentLocks
class	SIXLock
: public Lock
{
};

template<class Blocked>
bool
Lock::wait_while(Blocked blocked, const Milliseconds* timeout)
{
	long		remaining = timeout ? (long)timeout->ms() : 0;
	while (blocked())
	{
		if (!timeout)
			cond.wait(&latch);
		else if (remaining <= 0)
			return false;
		else
			cond.wait(remaining, &latch);
	}
	return true;
}

inline void
Lock::check_not_own(const Milliseconds* timeout) const
{
	StrppAssert(timeout || !writer || owner != Thread::currentId());
}

inline bool
Lock::take_shared(const Milliseconds* timeout)
{
	check_not_own(timeout);
	if (!wait_while([this]{ return writer; }, timeout))
		return false;
	++readers;
	return true;
}

inline bool
Lock::take_intent(const Milliseconds* timeout)
{
	check_not_own(timeout);
	if (!wait_while([this]{ return writer; }, timeout))
		return false;
	writer = true;
	owner = Thread::currentId();
	return true;
}

inline bool
Lock::drain(const Milliseconds* timeout)
{
	return wait_while([this]{ return readers > 0; }, timeout);
}

inline void
Lock::release_shared()
{
	latch.enter();
	if (--readers == 0)
		cond.broadcast();	// A writer may be waiting for this
	latch.leave();
}

inline void
Lock::release_writer()
{
	latch.enter();
	writer = false;
	cond.broadcast();
	latch.leave();
}

// A lock you hold. Empty if it was not granted, or after release() or a move.
class	SharedLock
{
public:
	explicit	SharedLock(SIXLock& a_lock)
			: lock(&a_lock)
			{ lock->latch.enter(); lock->take_shared(0); lock->latch.leave(); }
	SharedLock(SIXLock& a_lock, Milliseconds timeout)
			: lock(&a_lock)
			{
				lock->latch.enter();
				bool	granted = lock->take_shared(&timeout);
				lock->latch.leave();
				if (!granted)
					lock = 0;
			}
	SharedLock(SharedLock&& o) : lock(o.lock) { o.lock = 0; }
	~SharedLock()	{ release(); }

	SharedLock(const SharedLock&) = delete;
	SharedLock&	operator=(const SharedLock&) = delete;
	SharedLock&	operator=(SharedLock&& o)
			{ release(); lock = o.lock; o.lock = 0; return *this; }

	bool		holding() const	{ return lock != 0; }
	void		release()	{ if (lock) lock->release_shared(); lock = 0; }

private:
	Lock*		lock;
};

class	IntentLock
{
public:
	explicit	IntentLock(SIXLock& a_lock)
			: lock(&a_lock)
			{ lock->latch.enter(); lock->take_intent(0); lock->latch.leave(); }
	IntentLock(SIXLock& a_lock, Milliseconds timeout)
			: lock(&a_lock)
			{
				lock->latch.enter();
				bool	granted = lock->take_intent(&timeout);
				lock->latch.leave();
				if (!granted)
					lock = 0;
			}
	IntentLock(IntentLock&& o) : lock(o.lock) { o.lock = 0; }
	~IntentLock()	{ release(); }

	IntentLock(const IntentLock&) = delete;
	IntentLock&	operator=(const IntentLock&) = delete;
	IntentLock&	operator=(IntentLock&& o)
			{ release(); lock = o.lock; o.lock = 0; return *this; }

	bool		holding() const	{ return lock != 0; }
	void		release()	{ if (lock) lock->release_writer(); lock = 0; }

private:
	Lock*		lock;
	friend class	ExclLock;
};

class	ExclLock
{
public:
	// Wait for no other lock to be held
	explicit	ExclLock(Lock& a_lock)
			: lock(&a_lock)
			{
				lock->latch.enter();
				lock->take_intent(0);
				lock->drain(0);
				lock->latch.leave();
			}
	ExclLock(Lock& a_lock, Milliseconds timeout)
			: lock(&a_lock)
			{
				lock->latch.enter();
				bool	granted = lock->take_intent(&timeout) && lock->drain(&timeout);
				if (!granted && lock->writer && lock->owner == Thread::currentId())
				{			// Got the writer slot, but the readers stayed
					lock->writer = false;
					lock->cond.broadcast();
				}
				lock->latch.leave();
				if (!granted)
					lock = 0;
			}

	// Upgrade your IntentLock: it is empty afterwards, unless a timeout expired
	explicit	ExclLock(IntentLock&& intent)
			: lock(intent.lock)
			{
				StrppAssert(lock);
				lock->latch.enter();
				lock->drain(0);
				lock->latch.leave();
				intent.lock = 0;
			}
	ExclLock(IntentLock& intent, Milliseconds timeout)
			: lock(intent.lock)
			{
				StrppAssert(lock);
				lock->latch.enter();
				bool	granted = lock->drain(&timeout);
				lock->latch.leave();
				if (granted)
					intent.lock = 0;
				else
					lock = 0;	// You still hold the IntentLock
			}
	ExclLock(ExclLock&& o) : lock(o.lock) { o.lock = 0; }
	~ExclLock()	{ release(); }

	ExclLock(const ExclLock&) = delete;
	ExclLock&	operator=(const ExclLock&) = delete;
	ExclLock&	operator=(ExclLock&& o)
			{ release(); lock = o.lock; o.lock = 0; return *this; }

	bool		holding() const	{ return lock != 0; }
	void		release()	{ if (lock) lock->release_writer(); lock = 0; }

private:
	Lock*		lock;
};

#endif	// LOCK_H
