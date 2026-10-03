# strpp

A C++ library of reference-counted, copy-on-write value types with slices:
strings, arrays, maps and a tagged variant, together with the threading they
are built on.

- [Prologue](prologue.md)

## Reference

- [Unicode strings: the StrVal class](strval.md)
- [Raw Unicode character processing](unicode.md)
- [Array with slices](array.md)
- [Red-black trees: the RbTree class](redblack.md)
- [COWMap](cowmap.md)
- [Variant data type](variant.md)
- [Time and date: intervals and instants](datetime.md)
- [Gregorian: civil dates and ISO 8601](gregorian.md)
- [Error Management](error.md)
- [Message catalogs](messages.md)
- [Threads, locks and thread-local storage](threading.md)
- [MessageQueue: a thread's inbox](msgqueue.md)
- [Lock and SIXLock: shared, intent and exclusive locks](lock.md)
- [Transactional values and Windows](transactional.md)
- [The monitor: finding deadlocks, stalls and runaway buffers](monitor.md)
- [Pegexp](pegexp.md)
- [PEG parsing](peg.md)

## Rendering

Named `SUMMARY.md` because that is what mdBook reads to build its table of
contents. Any Markdown viewer shows it as an ordinary index.
