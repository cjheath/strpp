#if !defined(TRANSACTIONAL_H)
#define TRANSACTIONAL_H
/*
 * Transactional<T>: a value that you read and change through Windows.
 *
 * A Transactional<T> owns a T and a lock, and belongs to no thread. Any
 * thread can open a ReadWindow to read the value, or an UpdateWindow to
 * change it. Several ReadWindows can be open at once. An UpdateWindow is open
 * alone. Once an UpdateWindow is waiting, no new ReadWindow opens until it
 * has closed.
 *
 *	struct CounterData { CounterData() : count(0) {} int count; };
 *	Transactional<CounterData>	counter;
 *
 *	{
 *		UpdateWindow<CounterData>	w(counter);	// Waits for the readers
 *		w->count++;
 *	}
 *	ReadWindow<CounterData>		r(counter);	// Waits for any update
 *	use(r->count);
 *
 * A thread that publishes state holds a Transactional<T> as a member, and
 * its readers open ReadWindows on it. As it is the only writer, it can read
 * the value without a Window using unguarded(). Where there are several
 * writers, they must use a Window to read as well.
 *
 * Put what makes one consistent change in one UpdateWindow, and do the slow
 * work before you open it. Don't wait for anything while a Window is open,
 * and don't open a Window on a value while you hold an UpdateWindow on it.
 * A thread that wants no delay passes a timeout, and checks holding().
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<lock.h>

template<class T>
class	ReadWindow;
template<class T>
class	UpdateWindow;

template<class T>
class	Transactional
{
public:
	typedef T	Data;

	Transactional() {}
	explicit	Transactional(const T& initial) : value(initial) {}
	Transactional(const Transactional&) = delete;
	Transactional&	operator=(const Transactional&) = delete;

	// Call f(T&) in an UpdateWindow. Don't nest them: that waits for itself.
	template<class F>
	void		update(F f)
			{
				UpdateWindow<T>	w(*this);
				f(*w);
			}

	// The value with no Window. Only safe for the sole writer, or where nobody writes.
	const T&	unguarded() const	{ return value; }

private:
	SIXLock		lock;
	T		value;

	friend class	ReadWindow<T>;
	friend class	UpdateWindow<T>;
};

template<class T>
class	ReadWindow
{
public:
	// Wait for there to be no update
	explicit	ReadWindow(Transactional<T>& t)
			: tx(t), lock(t.lock)
			{}
	// Give up after the timeout; holding() says whether you have the Window
	ReadWindow(Transactional<T>& t, Milliseconds timeout)
			: tx(t), lock(t.lock, timeout)
			{}

	bool		holding() const	{ return lock.holding(); }
	void		close()		{ lock.release(); }

	const T*	operator->() const	{ StrppAssert(holding()); return &tx.value; }
	const T&	operator*() const	{ StrppAssert(holding()); return tx.value; }

private:
	Transactional<T>&	tx;
	SharedLock		lock;
};

template<class T>
class	UpdateWindow
{
public:
	// Wait for the readers and any other update to finish
	explicit	UpdateWindow(Transactional<T>& t)
			: tx(t), lock(t.lock)
			{}
	// Give up after the timeout; holding() says whether you have the Window
	UpdateWindow(Transactional<T>& t, Milliseconds timeout)
			: tx(t), lock(t.lock, timeout)
			{}

	bool		holding() const	{ return lock.holding(); }
	void		close()		{ lock.release(); }

	T*		operator->() const	{ StrppAssert(holding()); return &tx.value; }
	T&		operator*() const	{ StrppAssert(holding()); return tx.value; }

private:
	Transactional<T>&	tx;
	ExclLock		lock;
};

#endif	// TRANSACTIONAL_H
