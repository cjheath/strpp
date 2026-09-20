## The error buffer

`#include <errbuf.h>`

Every thread has one error buffer, holding the messages it has reported and
not yet dealt with. Nothing is formatted and nothing is decided about
language, style or severity when a message is reported: the buffer holds the
message number, its default text and its parameters, and whoever displays
the message decides all of that. That may be another thread or another
process entirely.

The design this implements is in `Notes/ErrorManagement.md`.

### Reporting

Generated code calls `Error` through a function per message:

	ErrNum	Error(ErrNum err, const char* default_text, VariantArray params);

It appends the message, and answers the number it was given, so that
reporting an error and returning it are one act:

	return ErrorADL_Syntax("foo", source.location);

The generated functions gather their arguments and call `Error`; there is
one of those per message, and one `Error` for all of them. A zero `ErrNum`
reports nothing.

### Reading

	MsgIndex	count() const;			// Messages held
	MsgSequence	checkpoint() const;		// The next number a report would take
	ErrNum		error(MsgIndex n) const;	// The number alone
	Message		message(MsgIndex n) const;	// Number, default text, parameters

`Message` is what someone about to deliver a message needs:

	struct Message
	{
		ErrNum		error;
		const char*	default_text;
		VariantArray	parameters;	// A slice of the buffer's parameter array
	};

Recovery usually wants only `error(n)`. Take a `Message` when you are about
to display one.

**A `Message`'s parameters share the buffer's parameter array, so let the
message go before delivering it.** While a slice is outstanding the array
cannot be reclaimed, and one held across a drain leaks that action's
parameters. `delivered()` and `clear()` assert that none is outstanding, so
the mistake stops rather than quietly leaking. In practice this means the
`Message` wants a scope of its own:

	{
		ErrBuf::Message	shown = buffer->message(0);
		display(shown);
	}
	buffer->delivered();

### Recovering

	MsgSequence	mark = buffer->checkpoint();
	...call something that may report...
	if (!wanted)
		buffer->rollback(mark);

`rollback` discards everything reported since the checkpoint, the callee
having no part in it. Messages the callee reported take their parameters
with them, so what remains stays contiguous.

Messages are consecutively numbered: a rollback gives its numbers back, and
the next report takes them again. A number is therefore only unique among
the messages currently held, which is all a checkpoint needs to be.

### Delivering

	while (buffer->count() > 0)
	{
		...read message(0) and let it go...
		buffer->delivered();		// The oldest has been delivered
	}

Delivery always drains the buffer. Delivering advances past the message
rather than compacting what follows, so it costs the same whatever is behind
it, and the last one to go empties both arrays while keeping their storage.
That is what makes the steady state free: **an action that reports and
delivers the same number of messages as the last one allocates nothing at
all.** `clear()` drops everything at once without delivering, retiring the
numbers.

### What a reporting function should do

Build the parameters in a scratch array that is emptied between reports
rather than rebuilt. A fresh `VariantArray` per report costs two
allocations, which is the cost this shape exists to avoid:

	static ThreadLocal<VariantArray>	scratch;

	VariantArray&	params = *scratch.get();
	params.evacuate();			// Empty, keeping the storage
	params.append(Variant(42));
	params.append(Variant("context"));
	return Error(SomeError, "the default text", params);

`evacuate()` is what keeps the storage; `clear()` would give it back.

### Threads and other processes

There is one buffer per thread, reached through a thread-local slot, so two
threads never see each other's messages. A caller running on one thread that
completes work for another - or a server answering a client - packs the
unformatted messages (number, default text and parameters) into whatever it
already speaks and ships them. The receiving context is the one that knows
the reader's language and the room there is to display in, so it is the one
that formats.

### Not implemented yet

- **Formatting.** What the substitution points look like, and what replaces
  printf-style directives, is not settled.
- **Catalogs.** Nothing yet reads a compiled catalog; the default text is
  what a message carries.
- **Severity.** A message has no severity recorded. Severity is contextual,
  and a warning may be demoted to information or raised by whoever displays
  it.
