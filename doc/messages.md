## Message catalogs

Messages to the user of a program should be concise, consistent, and complete.
They should be actionable, describing the problem, the reason and a solution.
A translator must know enough of the context to know how to phrase the text.
Parameters may occur in a different order in different translations.

Message catalog management keeps that in one place, not embedded in the code.
Numbers are allocated once and should not be reallocated. The completeness
of a translation and the types of the parameters should be checked at build.

We write message catalogs in ADL, a small type-checked declarative language
for describing structured data. The code and its message catalog are separate.
A generator produces two header files for the library build from.

### The catalog schema

ADL files are type-checked against a schema itself written in ADL. The
Message Catalog Source schema object contains `Set`s, each set holds
`Message`s with a number and default text, and may contain additional
Text objects for translations or styles.

```adl
Message Catalog Source:
{
	Set: {
		Prefix: String;			// Used in filenames and Error definitions

		Number: Integer;		// Globally allocated set number

		-> Languages.Language;		// Default language for the set

		Message: {
			Number: Integer;	// Must be unique in the Set, 1-1023
			Usage: String;		// Notes for a translator
			Syntax = String;	// Default text matches String's Syntax

			Text: {			    // Stylised text (Language or Style override)
				-> Languages.Language;	// Or default to the set's language
				Style: String;	// Typically blank/unset, could be "terse", "verbose", etc
				Syntax = String;// Allow direct assignment
			}
		}
	}
}
```

In ADL, any object which has a Syntax is assignable. Although Message is
a container, it can also be assigned directly, any value matching the
Syntax for a String. The above schema assumes a list of Languages defined
in a prior file. Normally only the default text is in the main file, with
translations provided by additional files.

### A catalog of default texts

The catalog's name is used to derive the names of the two generated header
files. Within it are Sets, and within those are Messages.  Here is part
of the library's catalog:

```adl
Strpp: Message Catalog Source {

	StrVal: Set {
		Prefix = 'STR';
		Number = 1;
		Language = Languages.en;

		Assert: Message { Number = 1; } ~= 'At {1}:{2}, assertion failed: `{3}`';

		Trailing text: Message { Number = 2; } ~= 'The number `{1}` in radix {2} ended at character {3}, leaving `{4}` unread';
		No digits: Message { Number = 3; } ~= 'Failed to read a number from `{1}` in radix {2}';
		...
		Is Complete = True;
	}
}
```

Each message has a name consisting of one or more words. From these words,
`msggen` emits the name of an ErrNum and the name of a function that
reports this message, expecting the correct parameters.  For example,
the message called `Trailing text` becomes the macro `STRERR_TRAILING_TEXT`
and the function `ErrorSTR_TrailingText`.

The parameters of the text are numbered `{1}`, `{2}` and so on, usually in
the order the text uses them (but always the order used by the generated
function). A translation may use the parameters in any order, even more
than once if needed.

A Set of Message number, once used, should not be re-used or re-numbered,
for the sake of customer support staff. Even to a user who cannot read the
text, the Set-Message number uniquely identifies a specific message condition.
Each set typically ends `Is Complete = True;` to prevent any translation
adding new content.

The value assignments using `=` are Final (cannot be overridden), whereas
the `~=` ones are tentative and may be overridden in a specified context.

### Translations

A translation is usually in a separate catalog file loaded after the first.
It re-opens the catalog by name and adds a new `Text` object to each Message
(Messages are not marked Complete for this reason). The message's default
text is not touched, so the two are independent and either may be reworded
alone. It is possible for a program to load multiple translations on top
of the same catalog.

```adl
// German wordings of strpp.mcs's messages.

deText: Set.Message.Text { Language = Languages.de; }

StrVal {
	Assert { : deText ~= 'In {1}:{2} schlug die Zusicherung fehl: `{3}`'; }
	Trailing text { : deText ~= 'Die Zahl `{1}` zur Basis {2} endete bei Zeichen {3}; `{4}` blieb ungelesen'; }
	...
}

Strpp;
```

Usually only one language is used per file, and a convenient way to do
that is to define a subtype of the schema's own `Message.Text` which sets
it once, so each message then only needs to add its text.

When loading ADL files, new declarations begin in the last closed scope from
the predecessor. To facilitate loading multiple languages, it is conventional
that the file ends with a bare `Strpp;`, which leaves the catalog as the
starting point for whatever file is read next.

### Generating the headers

The generated headers are built by `msggen`, which reads the schema, the
catalog and any translation files, and writes both headers. With `-d` it
writes them into a directory:

```
cd strpp/tools
./msggen -d ../include ../../adl/cpp/adl.adl ../../adl/cpp/ietf_languages.adl \
	mcs.adl ../messages/strpp.mcs
```

Every file named is loaded in the order given, each from the context the one
before it left: the language enumeration and the schema first, then the
catalog, since that is where the catalog's name and its sets come from. The
name of the object defined by the **last statement of the last file** is what
names the output files. For a catalog file that is the catalog itself, since
its own definition is its last statement - which is why a file that goes on
after its catalog generates from whatever it defined next.

Without `-d` the two headers go to standard output instead, so you can
pass your eye over them without writingh them to storage.

```
./msggen ../../adl/cpp/adl.adl ../../adl/cpp/ietf_languages.adl \
	mcs.adl ../messages/strpp.mcs
```

One of the headers is used by code needing to emit messages, the other
by consumers wishing to match ErrNum return codes. The parameters and
default text are pushed to the thread's error cascade, to be reported
or recoverd as previously discussed.

`strpp_err.h` gives each message a number and, in a comment, the default
text:

```c
#define	STRERR_SET			1	// The message set allocated to the STR

#define	STRERR_ASSERT		ErrNum(STRERR_SET, 1)	// At {1}:{2}, assertion failed: `{3}`
#define	STRERR_TRAILING_TEXT		ErrNum(STRERR_SET, 2)	// The number `{1}` in radix {2} ended at character {3}, leaving `{4}` unread
#define	STRERR_NUMBER_OVERFLOW		ErrNum(STRERR_SET, 4)	// The number `{1}` in radix {2} does not fit in `{3}`, overflowing at character {4}; the value returned is incomplete
```

An `ErrNum` holds a set number and a message number within it, and its
`#define` is the message's name. See [Error Management](error.md) for
details.

`strpp_msg.h` provides a function for each message. This function
takes the parameters, logs the error and returns the ErrNum:

```c++
inline ErrNum
ErrorSTR_Assert(Variant p1, Variant p2, Variant p3)
{
	return Error(STRERR_ASSERT,
		"At {1}:{2}, assertion failed: `{3}`",
		VariantArray() << p1 << p2 << p3);
}
```

This is all in the name of making it as easy to report an error with full
context than to just return an error number.

In an upcoming revision of this process, the function's parameters will
be typed and converted to Variant to be put into the error buffer.
