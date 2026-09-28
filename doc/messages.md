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
A generator writes two header files for the library to build from.

### The catalog schema

ADL type-checks your files against a schema, itself written in ADL. The
Message Catalog Source schema object holds `Set`s; each set holds `Message`s
with a number and default text, and may hold further `Text` objects for
translations or styles.

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

In ADL you can assign a value to any object that has a Syntax. A Message is
a container, but you can also assign text straight to it, since it matches
the Syntax for a String. The schema expects a list of Languages, from a file
loaded before it. Keep the default text in the main file, and put each
translation in an additional one.

### A catalog of default texts

The catalog's name gives the names of both generated headers. It holds Sets,
and each Set holds Messages. Here is part of the library's catalog:

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

A message's name is one or more words, and `msggen` builds both names from
them: the ErrNum macro and the function that reports the message. The
message called `Trailing text` becomes the macro `STRERR_TRAILING_TEXT` and
the function `ErrorSTR_TrailingText`.

Number the parameters `{1}`, `{2}` and so on in the order the text uses them,
and the generated function takes them in that order. A translation may use
them in any order, and may use one twice.

Once you use a number, never re-use or re-number it: the Set-Message number
identifies one condition for customer support, even to someone who cannot
read the text. Each set ends `Is Complete = True;`, which stops a translation
adding content to it.

Assign with `=` for Final, which nothing can override; or with `~=` for
tentative, which a specified context may override.

### Translations

Put each translation in a separate catalog file, loaded after the first. It
re-opens the catalog by name and adds a new `Text` object to each Message
(which is why you Complete a Set but not its Messages). It leaves the default
text alone, so you can reword either one. You can load several translations
over the same catalog.

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

Use one language per file, and define a subtype of the schema's own
`Message.Text` that sets it once - then each message needs only its text.

When ADL loads a file, new declarations begin in the scope the file before it
left closed. So end each translation with a bare `Strpp;`: that re-opens the
catalog, and leaves it as the starting point for the next file.

### Generating the headers

`msggen` reads the schema, the catalog and any translation files, and writes
both headers. Give it `-d` to write them into a directory:

```
cd strpp/tools
./msggen -d ../include ../../adl/cpp/adl.adl ../../adl/cpp/ietf_languages.adl \
	mcs.adl ../messages/strpp.mcs
```

`msggen` loads every file you name in the order you give, each from the
context the one before it left. Name the language enumeration and the schema
first, then the catalog, which is where the name and the sets come from. It
names the output files after the object the **last statement of the last
file** defines, which for a catalog file is the catalog itself - so a file
that goes on after its catalog generates from whatever it defined next.

Without `-d` the two headers go to standard output instead, so you can
pass your eye over them without writing them to storage.

```
./msggen ../../adl/cpp/adl.adl ../../adl/cpp/ietf_languages.adl \
	mcs.adl ../messages/strpp.mcs
```

Code that emits messages includes one header; code that matches ErrNum
return codes includes the other. Reporting pushes the parameters and the
default text onto the thread's error cascade, to be reported or recovered
as [Error Management](error.md) describes.

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

All of this makes reporting an error with full context easier than returning
a bare number.

A coming revision will type the function's parameters, and convert them to
Variant for the error buffer.
