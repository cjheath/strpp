#if !defined(WINDOW_H)
#define WINDOW_H
/*
 * Window: read-only access to the data a Thread keeps in its protected area.
 *
 * A thread that exposes data defines a struct for it, and derives from
 * Windowed<Owner, Data>. Only the owner writes the data, and only inside
 * update(); it reads it with data():
 *
 *	struct CounterData { CounterData() : count(0) {} int count; };
 *
 *	class Counter : public Thread, public Windowed<Counter, CounterData>
 *	{
 *		int run()
 *		{
 *			for (;;)
 *			{
 *				Variant message = MessageQueue::mine()->pop();
 *				int next = data().count + message.as_int();	// Slow work goes here
 *				update([&](Data& d) { d.count = next; });
 *			}
 *		}
 *	};
 *
 * Any other thread opens a Window on it and reads data through the Window:
 *
 *	Window<Counter> w(counter);	// Waits until the thread is not updating
 *	use(w->count);
 *					// Window closed here
 *
 * While any Window is open, the owner changes no data. Several Windows can be
 * open at once. Once the owner wants to update, no new Window opens until it
 * has finished, and then they may open again.
 *
 * Do the slow work before update(), and put what changes one consistent state
 * in one update(): a reader can open a Window between two of them. Don't wait
 * for anything inside it. Handle each message quickly; it holds up the
 * messages behind it whether or not anyone has a Window open.
 *
 * You must not wait for the owner while you hold a Window: it cannot update
 * until you close it. A thread that wants no delay passes a timeout, and
 * checks holding().
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<thread.h>
#include	<lock.h>

template<class Owner>
class	Window;

template<class Owner, class D>
class	Windowed
{
public:
	typedef D	Data;

	Windowed() {}
	Windowed(const Windowed&) = delete;
	Windowed&	operator=(const Windowed&) = delete;

protected:
	const Data&	data() const	// Only the owner may call this without a Window
			{ return window_data; }

	// Call f(Data&) with no Window open. Don't nest them: that waits for itself.
	template<class F>
	void		update(F f)
			{
				IntentLock	intent(window_lock);
				ExclLock	exclusive(static_cast<IntentLock&&>(intent));
				f(window_data);
			}

private:
	SIXLock		window_lock;
	Data		window_data;

	friend class	Window<Owner>;
};

template<class Owner>
class	Window
{
public:
	// Wait for the owner to be between updates
	explicit	Window(Owner& a_owner)
			: owner(a_owner), lock(lock_of(a_owner))
			{}
	// Give up after the timeout; holding() says whether you have the Window
	Window(Owner& a_owner, Milliseconds timeout)
			: owner(a_owner), lock(lock_of(a_owner), timeout)
			{}

	bool		holding() const	{ return lock.holding(); }
	void		close()		{ lock.release(); }

	const typename Owner::Data*	operator->() const
			{ StrppAssert(holding()); return &owner_data().window_data; }
	const typename Owner::Data&	operator*() const
			{ StrppAssert(holding()); return owner_data().window_data; }

private:
	typedef Windowed<Owner, typename Owner::Data>	Exposed;

	Owner&		owner;
	SharedLock	lock;

	static SIXLock&	lock_of(Owner& o)
			{ return static_cast<Exposed&>(o).window_lock; }
	const Exposed&	owner_data() const
			{ return static_cast<const Exposed&>(owner); }
};

#endif	// WINDOW_H
