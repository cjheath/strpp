#if !defined(ERRBUF_H)
#define ERRBUF_H
/*
 * The error buffer: what a thread has reported and not yet dealt with.
 *
 * One buffer per thread, reached through a thread-local slot. Reporting appends
 * an entry - the SM number, the default text, and a span into the buffer's
 * parameter array - and nothing else happens: no text is formatted, and no
 * decision about language, style or severity is taken. Those belong to the
 * context that will display the message, which may be another thread or another
 * process entirely. See Notes/ErrorManagement.md.
 *
 * The parameters live in one flat array that only ever grows, so reporting
 * costs a VariantArray being handed in and nothing being allocated: the array
 * is emptied with evacuate(), which keeps its storage for the next action.
 *
 * An entry's parameters are read by index rather than handed out as a slice.
 * A slice would share the parameter array's body, and the next report would
 * then have to copy the whole thing before it could append - which is the one
 * cost this shape exists to avoid.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<error.h>
#include	<variant.h>
#include	<thread_local.h>

class	ErrBuf
{
public:
	typedef	ArrayIndex	MsgIndex;	// A message's position in the buffer
	typedef	unsigned	MsgSequence;	// The number a message is known by
	typedef	ArrayIndex	ParamIndex;	// A position in the parameter array
	typedef	unsigned	ParamSequence;	// A position in the stream of parameters

	/*
	 * A message as whoever is about to deliver it wants it: what it is, and all
	 * of its parameters as a slice of the buffer's parameter array.
	 *
	 * The slice shares that array, so a message must have been let go before
	 * the delivered() that retires it: while a slice is outstanding the array
	 * cannot be reclaimed, and holding one across a drain leaks the action's
	 * parameters. delivered() and clear() assert that none is outstanding, so
	 * a mistake of that kind stops rather than quietly leaking.
	 */
	struct	Message
	{
		ErrNum		error;
		const char*	default_text;
		VariantArray	parameters;
	};

			ErrBuf()
			: first_entry(0), first_parameter(0), delivered_count(0) {}

	MsgIndex	count() const		{ return live(); }
	MsgSequence	checkpoint() const	{ return delivered_count + live(); }

	// What recovery almost always wants: the number and nothing else
	ErrNum		error(MsgIndex n) const
			{
				assert(n < live());
				return entries[first_entry+n].error();
			}

	// The whole message, for whoever is about to deliver it
	Message		message(MsgIndex n) const
			{
				Entry	e = entries[first_entry+n];
				Message	m;
				m.error = e.error();
				m.default_text = e.default_text();
				m.parameters = parameters.slice(e.first(), e.parameters());
				return m;
			}

	/*
	 * Append a reported message, and answer its sequence number. Messages are
	 * consecutively numbered: a recovery gives its numbers back, so the next
	 * report takes them again, and a delivered number is never re-used.
	 */
	MsgSequence	report(ErrNum err, const char* default_text, VariantArray params)
			{
				if (!err)
					return 0;	// No error: nothing to report

				ParamIndex	first = parameters.length();
				for (ParamIndex i = 0; i < params.length(); i++)
					parameters.append(params[i]);

				entries.append(Entry(err, default_text, first, params.length()));
				return delivered_count + live();
			}

	/*
	 * Discard everything reported since a checkpoint, parameters and all - so
	 * that the parameters of what remains stay contiguous, and the numbers the
	 * recovered messages held can be used again. A caller that did not want the
	 * outcome of a call drops what the callee reported, without the callee
	 * being party to it.
	 */
	void		rollback(MsgSequence which)
			{
				// Everything live is newer than the checkpoint, even where a
				// delivery has already carried the checkpoint's own message away
				MsgIndex	keep = which > delivered_count ? which - delivered_count : 0;
				if (keep >= live())
					return;

				// The recovered messages are the last ones, and so are their
				// parameters: drop just those, leaving the rest where they are
				ParamIndex	at = entries[first_entry+keep].first();
				entries.remove(first_entry+keep, live()-keep);
				parameters.remove(at, parameters.length()-at);
			}

	/*
	 * The oldest message has been delivered: take it out of the buffer. This
	 * advances past it rather than compacting what follows, so it costs the
	 * same whatever is behind it - and the last one to go evacuates both
	 * arrays, which empties them while keeping the storage for the next action
	 * to reuse. That is what bounds the storage: an action's worth, reused.
	 */
	void		delivered()
			{
				assert(live() > 0);
				assert(!parameters.isShared());	// No Message still holding a slice
				delivered_count += 1;		// Its number is retired for good
				if (live() == 1)
				{
					entries.evacuate();
					parameters.evacuate();
					first_entry = first_parameter = 0;
					return;
				}
				first_parameter += entries[first_entry].parameters();
				first_entry++;
			}

	// Drop everything, keeping the storage for the next action to reuse
	void		clear()
			{
				assert(!parameters.isShared());	// No Message still holding a slice
				delivered_count += live();
				entries.evacuate();
				parameters.evacuate();
				first_entry = first_parameter = 0;
			}

private:
	/*
	 * One reported message: what it is, and where its parameters are. There is
	 * deliberately no severity here; severity is contextual, and belongs to
	 * whoever displays the message.
	 */
	class	Entry
	{
	public:
		Entry()
		: num(), text(0), first_param(0), count(0) {}
		Entry(ErrNum a_num, const char* a_text, ParamIndex a_first, ParamIndex a_count)
		: num(a_num), text(a_text), first_param(a_first), count(a_count) {}

		ErrNum		error() const		{ return num; }
		const char*	default_text() const	{ return text; }
		ParamIndex	parameters() const	{ return count; }
		ParamIndex	first() const		{ return first_param; }

	private:
		ErrNum		num;
		const char*	text;		// Statically compiled in: see section 7
		ParamIndex	first_param;	// Absolute in the parameter array
		ParamIndex	count;
	};

	MsgIndex	live() const		{ return entries.length() - first_entry; }

	Array<Entry>	entries;		// Delivered ones are behind first_entry
	MsgIndex	first_entry;
	VariantArray	parameters;		// Delivered ones stay where they are:
	ParamIndex	first_parameter;	// first_parameter only counts them off
	MsgSequence	delivered_count;	// Messages delivered over all time
};

/*
 * The global error API that the generated reporting functions call, each with
 * its own message's SM number and default text. This is where the appending
 * happens, and it answers the number it was given, so that reporting an error
 * and returning it are one act.
 *
 * REVISIT: the name is mine, not the author's - to settle with the rest of the
 * generated API.
 */
ErrNum	ErrReport(ErrNum err, const char* default_text, VariantArray params);

// This thread's error buffer, made on first use
ThreadLocal<ErrBuf>&	error_buffer();

#endif	// ERRBUF_H
