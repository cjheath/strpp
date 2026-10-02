/*
 * The error buffer: what it holds, and the global API that reports into it.
 *
 * The buffer's own methods are here rather than in the header, which everything
 * that reports includes: one copy of them in the library instead of one in each
 * translation unit.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<errbuf.h>
#include	<thread.h>

ErrBuf::ErrBuf()
: first_entry(0), first_parameter(0), delivered_count(0)
#if	defined(STRPP_MONITOR)
, stat_live(0), stat_parameters(0), stat_reported(0), owner(Thread::currentId()), registry_next(0)
#endif
{
#if	defined(STRPP_MONITOR)
	Registry<ErrBuf>::instance().add(this);
#endif
}

#if	defined(STRPP_MONITOR)
ErrBuf::~ErrBuf()
{
	Registry<ErrBuf>::instance().remove(this);
}

void
ErrBuf::publish()
{
	stat_live = (unsigned)live();
	stat_parameters = (unsigned)(parameters.length() - first_parameter);
}

unsigned
ErrBuf::snapshot(ErrBufRecord* out, unsigned max_out)
{
	unsigned	n = 0;
	Registry<ErrBuf>::instance().each([&](ErrBuf& b)
	{
		if (n >= max_out)
			return;
		ErrBufRecord&	r = out[n++];
		r.buffer = &b;
		r.owner = b.owner;
		r.live = b.stat_live;
		r.parameters = b.stat_parameters;
		r.reported = b.stat_reported;
	});
	return n;
}
#define	ERRBUF_PUBLISH()	publish()
#else
#define	ERRBUF_PUBLISH()	((void)0)
#endif

ErrBuf::MsgIndex
ErrBuf::live() const
{
	return entries.length() - first_entry;
}

ErrBuf::MsgIndex
ErrBuf::count() const
{
	return live();
}

ErrBuf::MsgSequence
ErrBuf::checkpoint() const
{
	return delivered_count + live();
}

ErrNum
ErrBuf::error(MsgIndex n) const
{
	assert(n < live());
	return entries[first_entry+n].error();
}

ErrBuf::Message
ErrBuf::message(MsgIndex n) const
{
	Entry	e = entries[first_entry+n];
	Message	m;
	m.error = e.error();
	m.default_text = e.default_text();
	m.parameters = parameters.slice(e.first(), e.parameters());
	return m;
}

ErrBuf::MsgSequence
ErrBuf::report(ErrNum err, const char* default_text, VariantArray params)
{
	if (!err)
		return 0;	// No error: nothing to report

	ParamIndex	first = parameters.length();
	for (ParamIndex i = 0; i < params.length(); i++)
		parameters.append(params[i]);

	entries.append(Entry(err, default_text, first, params.length()));
#if	defined(STRPP_MONITOR)
	stat_reported++;
#endif
	ERRBUF_PUBLISH();
	return delivered_count + live();
}

void
ErrBuf::rollback(MsgSequence which)
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
	ERRBUF_PUBLISH();
}

void
ErrBuf::delivered()
{
	assert(live() > 0);
	assert(!parameters.isShared());	// No Message still holding a slice
	delivered_count += 1;		// Its number is retired for good
	if (live() == 1)
	{
		entries.evacuate();
		parameters.evacuate();
		first_entry = first_parameter = 0;
		ERRBUF_PUBLISH();
		return;
	}
	first_parameter += entries[first_entry].parameters();
	first_entry++;
	ERRBUF_PUBLISH();
}

void
ErrBuf::clear()
{
	assert(!parameters.isShared());	// No Message still holding a slice
	delivered_count += live();
	entries.evacuate();
	parameters.evacuate();
	first_entry = first_parameter = 0;
	ERRBUF_PUBLISH();
}

/*
 * One slot for the process, claimed as this object is constructed. A namespace
 * scope static rather than a function-local one, which would need a
 * thread-safe-initialisation guard on every call - and FreeRTOS builds may not
 * have the locks that guard wants. Nothing may report an error before static
 * initialisation has run.
 */
static ThreadLocal<ErrBuf>	error_buffer_tls;

ErrBuf*
ErrBuffer()
{
	return error_buffer_tls.get();
}

ErrBuf*
ErrBuf::peek_mine()
{
	return error_buffer_tls.peek();
}

ErrBuf*
ErrBuf::detach_mine()
{
	ErrBuf*		b = error_buffer_tls.release();
	if (b && b->count() == 0)
	{
		delete b;
		return 0;
	}
	return b;
}

ErrNum
Error(ErrNum err, const char* default_text, VariantArray params)
{
	ErrBuffer()->report(err, default_text, params);
	return err;
}
