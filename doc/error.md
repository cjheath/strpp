## Error Management

User-focussed management of errors requires that the user is provided with
all relevant context to an error, not just a simple error code with a
generic message. Where one error cascades into another that can provide more
context, or a path to recovery, that must be provided, with neither error
report masking the others. Failure to properly report error context and a
path to making progress has been a major cause of inconvenience to users
throughout the history of the software industry.

This level of error management is tedious to implement with a bare Unix
errno or a Windows HRESULT. If a "file not found" message must be emitted,
the message must say what file or directory component was the cause of the
search path failure. If "access is denied", it must say what access was
requested, to what object, and on what basis the request was denied.

An error here carries its parameters in full, preferably with a structured
presentation of the problem, reason and solution - what went wrong, why it
went wrong, and what can be done about it. Cascading errors build a
tombstone from which the user can discern the original cause as well as the
end result, and is given every assistance in fixing it.  They are never
left guessing, and that is a design goal for the whole system rather than a
property of any one part of it.

The second goal is to make it awkward for a programmer to return a bare error
code. A code by itself tells the reader nothing, and an error is worth
reporting only with the parameters that allow it to be avoided in future.
Since the call that records the error is also the call that produces the
value returned, the ordinary way to return an error is the way that records
it. Awkward rather than impossible: a very few places justify returning the
code alone, so it has to stay possible; what the shape must avoid is the
bare code being the easy thing to write.

None of this is justification for an expensive implementation, because it
may have to run on very limited hardware. An error which can be recovered is
never formatted into text, only the minimum work is done. The caller who
receives an error code can ignore the presence of message text and
parameters, and just act on the error code, or they can recover by deleting
the reported error.

So reporting a failure takes two things: an `ErrNum` names what went wrong,
and an entry in a per-thread buffer accumulates what has been reported but
not yet recovered or delivered as a message. The error number is compact,
just a 32-bit compile-time constant, so error handling can use `switch`
cases.

### Error numbers: the ErrNum type

`#include	<error.h>`

An ErrNum is a 32-bit value naming a message: a set number, and a number
within that set. It subsumes both errno and Microsoft's HRESULT, so one
value carries a failure from anywhere - ours, the C library's, or Windows'
without the reader needing to know which.

ErrNum is a class rather than a bare integer, so the tests over it are
methods:

	ErrNum	e = ...;
	if (e.is_failure())		// Something went wrong
		...
	if (e.is_info())		// Worth knowing, but not a failure
		...

Its constexpr cast to int32_t allows the values in a switch, which is how a
caller handles the errors it knows and passes on the rest:

	ErrNum e = DoSomeWork();
	switch (e)
	{
	case 0:
		// That seemed to go ok
		break;
	case SubsysErrorOfSomeTypeNum:	// A generated name for a set's message
		// Handle the error
		Complain(e);
		return;
	default:
		return e;		// Not ours to handle
	}

Set and message numbers come from a message set description: a set's messages
become `#define`s in a generated header, `<Module>_err.h`, one per message with
the default text in a comment, so that code has names rather than numbers. Its
companion, `<Module>_msg.h`, holds the reporting function of each - the pair is
for the two things a message is for, and a translation is written against the
texts the first half carries.

One library has one message source file, however many sets are in it. This
library's is [str_err.h](https://github.com/cjheath/strpp/blob/main/include/str_err.h)
and [str_msg.h](https://github.com/cjheath/strpp/blob/main/include/str_msg.h),
which hold the sets `STR`, for the strings, and `VAR`, for the Variant. A set
carries what can go wrong within it and nothing else, which is why a message
about reading a number from a text is in `STR` rather than beside the class it
came from. There are 1024 messages to a set, and set numbers run to 262143.

A number, once used, is never re-used, so a number written into a log, a
manual or a customer's report keeps its meaning over the life of the
software.

`is_failure` and `is_info` are the intended API but their names are not yet
settled.

#### Public methods

Defined in [error.h](https://github.com/cjheath/strpp/blob/main/include/error.h).

- `ErrNum()` - the zero value, which is success.
- `ErrNum(int set, int msg)` - a failure, from a message set number and a
  message number within it.
- `ErrNum(int32_t system)` - a system's own number, errno or HRESULT, wrapped
  exactly as it stands.
- `set()`, `msg()` - the message set number, and the number within that set.
- `is_failure()`, `is_info()` - whether the failure bit or the information bit
  is set.
- `operator int32_t()` - the raw number, which is what lets the values be used
  in a switch.
- `operator==`, `operator!=`, `operator<`, `operator>` - compared by number,
  against another ErrNum or against an integer.

The bits it is built from are public too: `ERR_FLAG` marks a failure,
`ERR_INFO` information or a warning, `ERR_CUST` is set on ours so that no
Microsoft subsystem can collide with them, and `ERR_RSVD` must stay clear.

### The error buffer

`#include	<errbuf.h>`

Every thread has one error buffer, holding the messages it has reported and
not yet dealt with. Nothing is formatted and nothing is decided about
language, style or severity when a message is reported: the buffer holds the
message number, its default text and its parameters, and whoever displays
the message decides all of that. That may be another thread or another
process entirely.

#### Public methods

Defined in [errbuf.h](https://github.com/cjheath/strpp/blob/main/include/errbuf.h).

- `count()` - the messages it holds, reported and not yet dealt with.
- `checkpoint()` - the number the next report would take, which is what you
  keep to roll back to.
- `error(MsgIndex n)` - the number of the nth message, and nothing else. What
  recovery usually wants.
- `message(MsgIndex n)` - the nth message: its number, default text and
  parameters.
- `report(ErrNum err, const char* default_text, VariantArray params)` - append
  a message and answer its sequence number. A zero ErrNum reports nothing.
- `rollback(MsgSequence which)` - discard everything reported since that
  checkpoint, the parameters going with it.
- `delivered()` - retire the oldest message, once you have delivered it.
- `clear()` - drop everything at once, keeping the storage.
- `Error(ErrNum err, const char* default_text, VariantArray params)` - the
  free function the generated reporting functions call: it reports into this
  thread's buffer and answers the number, so reporting and returning are one
  act.
- `error_buffer()` - the free function that answers this thread's buffer,
  making it on first use.

#### Reporting

Generated code calls `Error` through a function per message:

	ErrNum	Error(ErrNum err, const char* default_text, VariantArray params);

It appends the message, and answers the number it was given, so that
reporting an error and returning it are one act:

	return ErrorADL_Syntax("foo", source.location);

The generated functions gather their arguments and call `Error`; there is
one of those per message, and one `Error` for all of them. A zero `ErrNum`
reports nothing.

#### Reading

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

#### Recovering

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

#### Delivering

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

#### What a reporting function should do

Build the parameters where they are used, as an array handed to the reporting
call:

	return Error(SomeError, "the default text", VariantArray() << 42 << "context");

There is no scratch array and no thread-local slot for one: a report costs its
parameters and nothing else, and one report may be made from within another.
The default text names its substitution points by position, and carries what
belongs around them - backticks for a name, slashes for a syntax - so that a
translation can place them where its own language wants them. What replaces
printf-style directives is [substituting parameters into a
text](strval.md), which is what renders a message when it is displayed.

#### Threads and other processes

There is one buffer per thread, reached through a thread-local slot, so two
threads never see each other's messages. A caller running on one thread that
completes work for another - or a server answering a client - packs the
unformatted messages (number, default text and parameters) into whatever it
already speaks and ships them. The receiving context is the one that knows
the reader's language and the room there is to display in, so it is the one
that formats.

#### When a program must not continue

A report is not a decision to stop: the library returns the number and carries
on, and what the caller does with it is the caller's business. Where carrying
on would be wrong - a count its own type could not hold, a table that does not
match its keys - the library asserts instead, with `StrppAssert`: the failed
condition is reported to the same buffer as a message, including the file and
line it failed at, that buffer is dumped, and the program aborts. A failure
while the dump is running aborts at once, without reporting or dumping again,
so that a fault in the reporting path cannot become a loop.

Where the dump goes is the application's to say, since only it knows what a
developer will see. `strpp_panic_write` is a function pointer: the library
sets it to write to standard error where there is a `write(2)`, and to write
nowhere at all where there is not. See
[strassert.h](https://github.com/cjheath/strpp/blob/main/include/strassert.h).

#### Not implemented yet

- **Catalogs.** Nothing yet reads a compiled catalog; the default text is
  what a message carries.
- **Severity.** A message has no severity recorded. Severity is contextual,
  and a warning may be demoted to information or raised by whoever displays
  it.
