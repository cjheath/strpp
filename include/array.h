#if !defined(ARRAY_H)
#define ARRAY_H
/*
 * Array slices:
 * - All access to an array Body is via a lightweight slice.
 * - Slices should be passed by copying (cheap!) not by pointers or references.
 * - A slice is mutable, but does not affect the base array or its other slices (by-value semantics)
 * - Slices share Arrays with thread-safety and garbage collection using atomic reference counting
 *
 * A shared array Body cannot be mutated, it's always copied first to ensure it has only one Slice.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<cstdlib>
#include	<cstdint>
#include	<functional>

#include	<refcount.h>

#define ArrayIndexBits	16
typedef typename std::conditional<(ArrayIndexBits <= 16), uint16_t, uint32_t>::type  ArrayIndex;

template<typename E, typename I = ArrayIndex>	class	ArrayBody;
template<typename E, typename I = ArrayIndex, typename Body = ArrayBody<E, I>>	class	Array;

template<typename E, typename I, typename Body> class	Array
{
public:
	using	Element = E;
	using	Index = I;

	~Array() {}			// Destructor
	Array()				// Empty array
			: body(0), offset(0), num_elements(0) {}
	Array(const Array& s1)		// Normal copy constructor
			: body(s1.body), offset(s1.offset), num_elements(s1.num_elements) {}
	Array& operator=(const Array& s1) // Assignment operator
			{ body = s1.body; offset = s1.offset; num_elements = s1.num_elements; return *this; }
	Array(const Element* data, Index size, Index allocate = 0)	// construct by copying data
			: body(0), offset(0), num_elements(size)
			{
				if (allocate < size)
					allocate = size;
				body = new Body(data, true, size, allocate);
				num_elements = size;
			}
	Array(Body* _body)		// New reference to same Body; used for static strings
			: body(_body), offset(0), num_elements(_body->length()) {}

	Index		length() const
			{ return num_elements; }
	bool		isEmpty() const
			{ return length() == 0; }
	inline bool	isShared() const
			{ return body && body->GetRefCount() > 1; }

	// Access the elements:
	Element		operator[](int elem_num)
			{ assert(elem_num >= 0 && elem_num < num_elements && body);
			  return body->data()[offset+elem_num]; }			// Returns a copy
	const Element&	operator[](int elem_num) const
			{ assert(elem_num >= 0 && elem_num < num_elements && body);
			  return body->data()[offset+elem_num]; }
	const Element*	asElements() const { return body ? body->data()+offset : 0; }	// The caller must ensure to enforce bounds
	const Element&	set(int elem_num, Element& e)
			{ assert(elem_num >= 0 && elem_num < num_elements);
			  Unshare();
			  (*body)[elem_num] = e;
			  return (*body)[elem_num];
			}

			operator const Body&() const { return static_cast<const Body&>(body); }
	const Body*	operator->() const { return body; }
	const Body&	operator*() const { return *body; }

	// Comparisons:
	int		compare(const Array& comparand) const
			{
				Index	i;
				for (i = 0; i < num_elements && i < comparand.length(); i++)
				{
					// REVISIT: C++20 now provides the spaceship operator to avoid the double comparison.
					if (operator[](i) < comparand[i])
						return -1;
					if (comparand[i] < operator[](i))
						return 1;
				}
				if (i < comparand.length())
					return -1;
				return 0;
			}
	inline bool	operator==(const Array& comparand) const {
				if (length() != comparand.length())
					return false;

				for (Index i = 0; i < num_elements && i < comparand.length(); i++)
					if (!(operator[](i) == comparand[i]))
						return false;
				return true;
			}
	inline bool	operator!=(const Array& comparand) const { return !(*this == comparand); }

	// Extract sub-slices:
	Array		slice(Index at, int len = -1) const
			{
				assert(len >= -1);

				// Quick check for a null slice:
				if (at < 0 || at >= num_elements || len == 0)
					return Array();

				// Clamp slice length:
				if (len == -1)
					len = num_elements-at;
				else if (len > num_elements-at)
					len = num_elements-at;

				return Array(body, offset+at, len);
			}
	Array		head(Index num_elem) const
			{ return slice(0, num_elem); }
	Array		tail(Index num_elem) const
			{ return slice(length()-num_elem, num_elem); }
	Array		shorter(Index num_elem) const	// all elements up to tail
			{ return slice(0, length()-num_elem); }

	// Linear search for an element
	int		find(const Element& e, int after = -1) const
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp + (after < 0 ? 0 : after+1); bp < ep; bp++)
					if (*bp == e)
						return bp-dp;
				return -1;				// Not found
			}
	int		rfind(const Element& e, int before = -1) const
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp + (before < 0 ? num_elements : before)-1; bp > dp; bp--)
					if (*bp == e)
						return bp-dp;
				return -1;				// Not found
			}

	Array		operator+(const Array& addend) const		// Add, producing a Slice on a new Array
			{
				// Handle the rare but important case of extending a slice with a contiguous slice of the same body
				if ((Body*)body == (Body*)addend.body	// From the same body
				 && offset+num_elements == addend.offset)	// And this ends where the addend starts
					return Array(body, offset, num_elements+addend.num_elements);

				Array		newarray(asElements(), num_elements, num_elements+addend.length());
				newarray += addend;
				return newarray;
			}
	Array		operator+(const Element& addend) const
			{
				Array	newarray(asElements(), num_elements, num_elements+1);
				newarray += addend;
				return newarray;
			}

	void		clear() { num_elements = 0; }
	Array		drop(Index n) const
			{
				assert(num_elements >= n);
				Array	dropped = *this;
				return dropped.remove(0, n);
			}

	// Following methods usually mutate the current slice so must Unshare (copy) it.

	Array&		remove(Index at, int len = -1)			// Delete a section from the middle
			{
				if (len == -1)
					len = num_elements-at;
				assert(num_elements-len >= at);		// Care with unsigned arithmetic
				if (at == num_elements || len == 0)
					return *this;

				if (at+len == num_elements)
				{		// Shorten this slice at the end
					num_elements -= len;
					return *this;
				}
				if (at == 0)
				{		// Shorten this slice at the start
					offset += len;
					num_elements -= len;
					return *this;
				}

				Unshare();
				body->remove(at, len);
				num_elements -= len;
				return *this;
			}
	Element		delete_at(Index at)
			{
				Element	e = this->operator[](at);
				remove(at, 1);
				return e;
			}
	Array&		operator+=(const Array& addend)			// Add, Array is modified:
			{
				if (num_elements == 0 && !addend.noCopy())
					return *this = addend;		// Just assign, we were empty anyhow

				append(addend);
				return *this;
			}
	Array&		operator+=(const Element& addend)
			{
				Unshare(1);
				body->insert(num_elements, &addend, 1);
				num_elements++;
				return *this;
			}
	Array&		operator<<(const Element& addend)
			{ return *this += addend; }
	Array&		push(const Element& addend)
			{ return *this += addend; }
	Element		pull()
			{ assert(num_elements > 0); return this->operator[](--num_elements); }
	Element		last()
			{ assert(num_elements > 0); return this->operator[](num_elements-1); }
	Element		shift()
			{ assert(num_elements > 0); return delete_at(0); }
	void		unshift(const Element& e)
			{ Unshare(); body->insert(0, e); num_elements++; }
	void		insert(Index pos, const Array& addend)
			{
				if ((Body*)body == (Body*)addend.body)	// From the same body
				{
					// Handle the rare but important cases of extending a slice
					// with a contiguous slice of the same body, at the end:
					if (pos == num_elements			// Appending at the end
					 && offset+pos == addend.offset)	// addend starts where we end
					{
						num_elements += addend.length();
						return;
					}

					// The same thing, but at the start.
					if (pos == 0				// Inserting at the start
					 && addend.offset+addend.num_elements == offset)	// addend ends where we start
					{
						offset -= addend.length();
						num_elements += addend.length();
						return;
					}
				}

				Unshare(addend.length());
				body->insert(pos, addend.asElements(), addend.length());
				num_elements += addend.length();
			}
	void		append(const Array& addend)
			{ insert(num_elements, addend.asElements(), addend.length()); }
	void		reverse()
			{
				Unshare();
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				while (ep-dp > 1)				// We haven't reached the middle yet
				{
					Element	e = *--ep;
					*ep = *dp++;
					dp[-1] = e;
				}
			}
	void		delete_if(std::function<bool(const Element& e)> condition)
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	bp = dp;			// Output pointer
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* sp = dp; sp < ep; sp++, bp++)
					if (condition(*sp))
					{
						if (isShared())
						{
							Unshare();	// This invalidates the pointers; reset them
							dp = body->data() + (dp-sp);
							bp = body->data() + (bp-sp);
							ep = body->data() + (ep-sp);
							// sp = body->data();	// Unnecessary, we won't Unshare twice
						}
						bp--;	// Skip this element in the output
					}
				if (bp < dp)
					num_elements -= (dp-bp);
			}

	// Functional methods (these don't mutate or Unshare the subject):
	// Linear search for an element using a match function
	void		each(std::function<void(const Element& e)> operation) const
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp; bp < ep; bp++)
					operation(*bp);
			}
	bool		all(std::function<bool(const Element& e)> condition) const	// Do all elements satisfy the condition?
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp; bp < ep; bp++)
					if (!condition(*bp))
						return false;
				return true;
			}
	bool		any(std::function<bool(const Element& e)> condition) const	// Does any element satisfy the condition?
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp; bp < ep; bp++)
					if (condition(*bp))
						return true;
				return false;
			}
	bool		one(std::function<bool(const Element& e)> condition) const	// Exactly one element satisfies the condition
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				bool		found = false;
				for (const Element* bp = dp; bp < ep; bp++)
					if (condition(*bp))
					{
						if (found)
							return false;
						found = true;
					}
				return found;
			}
	int		find(std::function<bool(const Element& e)> match, int after = -1) const
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp + (after < 0 ? 0 : after+1); bp < ep; bp++)
					if (match(*bp))
						return bp-dp;
				return -1;				// Not found
			}
	int		rfind(std::function<bool(const Element& e)> match, int before = -1) const
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp + (before < 0 ? num_elements : before)-1; bp > dp; bp--)
					if (match(*bp))
						return bp-dp;
				return -1;				// Not found
			}
	int		detect(std::function<bool(const Element& e)> condition) const	// Return index of first element for which condition is true, or -1 if none
			{ return find(condition, -1); }
	Array		select(std::function<bool(const Element& e)> condition) const
			{
				Array	selected;
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp; bp < ep; bp++)
					if (condition(*bp))
						selected.append(*bp);
				return selected;
			}
	template<typename E2, typename I2> Array<E2, I2>	map(std::function<E2(const Element& e)> map1) const
			{
				Array<E2, I2>	output;
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp; bp < ep; bp++)
					output.append(map1(*bp));
				return output;
			}
	template<typename J> J	inject(const J& start, std::function<J(J&, const Element& e)> injection) const
			{
				J	accumulator = start;
				const Element*	dp = body->data()+offset;	// Start of our slice
				const Element*	ep = dp+num_elements;		// End of our slice
				for (const Element* bp = dp; bp < ep; bp++)
					accumulator = injection(accumulator, *bp);
				return accumulator;
			}
	int		bsearch(std::function<int(const Element& e)> comparator) const
			{
				const Element*	dp = body->data()+offset;	// Start of our slice
				Index		l = 0;
				Index		r = num_elements;
				while (r > l)
				{
					Index	m = l+(r-l+1)/2;	// Care with unsigned overflow
					int	c = comparator(dp[m]);
					if (c == 0)
						return m;
					if (c > 0)
						r = m-1;		// The middle element was too great
					else
						l = m+1;		// The middle element was too small
				}
				return -1;
			}
	// REVISIT: Not yet implemented (would be O(n^2) without hashing)
	// Array	uniq(std::function<int(const Element& e1, const Element& e2)> comparator) const;
	// void		sort(std::function<int(const Element& e1, const Element& e2)> comparator);

#if 0	// Not yet implemented
	void		transform(const std::function<Array(const Element*& cp, const Element* ep)> xform, int after = -1)
			{
				Unshare();
				body->transform(xform, after);
				num_elements = body->length();
			}
#endif

protected:
	Array(Body* body, Index offs, Index len)	// offs/len not bounds-checked!
			: body(body), offset(offs), num_elements(len) {}
	bool		noCopy() const;

public:

private:
	Ref<Body>	body;		// The storage structure for the elements
	Index		offset;		// Character number of the first element of this substring
	Index		num_elements;	// How many element in this substring

	void		Unshare(Index extra = 0)	// Get our own copy of Body that we can safely mutate
			{
				if (body && body->GetRefCount() <= 1)
					return;

				// Copy only this slice of the body's data, and reset our offset to zero
				body = new Body(asElements(), true, num_elements, num_elements+extra);
				offset = 0;
			}
};

template<typename E, typename I> class	ArrayBody
: public RefCounted			// This object will be deleted when the ref_count decrements to zero
{
public:
	using		Element = E;
	using		Index = I;
	using		A = Array<E, I>;

	~ArrayBody()
			{
				if (start
				 && num_alloc > 0)		// Don't delete borrowed data
					delete[] start;
			}
	ArrayBody()
			: start(0), num_elements(0), num_alloc(0) { }
	ArrayBody(const Element* data, bool copy, Index length, Index allocate = 0)
			: start(0)
			, num_elements(0)
			, num_alloc(0)
			{
				if (copy)
				{
					if (allocate < length)
						allocate = length;
					resize(allocate);
					// Copy using the copy constructor belonging to Element
					for (Index i = 0; i < length; i++)
						start[i] = data[i];
					num_elements = length;
				}
				else
				{
					start = (Element*)data;	// Cast const away; copy==false implies no change will occur
					num_elements = length;
					AddRef();		// Cannot be deleted or resized
				}
			}

	bool		noCopy() const { return num_alloc == 0; }	// This body or its data are transient (borrowed)

	const Element&	operator[](int elem_num) const { assert(elem_num >= 0 && elem_num < num_elements); return start[elem_num]; }
	Element&	operator[](int elem_num) { assert(elem_num >= 0 && elem_num < num_elements); return start[elem_num]; }

	inline Index	length() const { return num_elements; }
	const Element*	data() const { return start; }			// fast access
	const Element*	end() const { return start+num_elements; }		// ptr to the trailing NUL (if any)

	// Mutating methods. Must only be called when refcount <= 1 (i.e., unshared)
	void		insert(Index pos, const Element* elements, Index num)	// Insert a subarray
			{
				assert(ref_count <= 1);
				assert(num_elements >= pos);
				resize(num_elements + num);

				if (num_elements > pos)		// Move data up
					for (Index i = num_elements+num; i && i > pos+num; --i)
						start[i-1] = start[i-1-num];
				if (num > 0)
					for (Index i = 0; i < num; i++)
						start[pos+i] = elements[i];
				num_elements += num;
			}
	void		remove(Index at, int len = -1)		// Delete a subslice from the middle
			{
				assert(ref_count <= 1);
				assert(len >= -1);
				assert(at >= 0);
				assert(at < num_elements);

				if (len == -1)
					len = num_elements-at;
				for (Index i = at; i < num_elements-len; i++)
					start[i] = start[i+len];	// Use assignment operators
				// Note that the len elements after num_elements will not be destroyed until the array is deleted
				num_elements -= len;		// len says how many we deleted.
			}

#if 0	// Not yet implemented
	/*
	 * At every Element after the given point, the passed transform function can
	 * extract any number of elements (up to ep) and return a replacement Array for those elements
	 * To quit, leaving the remainder untransformed, return without advancing cp
	 * (but a returned Array will still be inserted).
	 * None of the returned Arrays will be retained, so they can use static Bodys (or the same Body)
	 */
	void		transform(const std::function<A(const A*& cp, const A* ep)> xform, int after = -1);
#endif

protected:
	Element*	start;		// start of the character data
	Index		num_elements;	// Number of elements
	Index		num_alloc;	// How many elements are allocated. 0 means data is not allocated so must not be freed

	void		resize(size_t minimum)	// Change the memory allocation
			{
				if (minimum <= num_alloc)
					return;

				minimum = ((minimum-1)|0x7)+1;	// round up to multiple of 8
				if (num_alloc)	// Minimum growth 50% rounded up to nearest 16
					num_alloc = ((num_alloc*3/2) | 0xF) + 1;
				if (num_alloc < minimum)
					num_alloc = minimum;		// Still not enough, get enough
				Element*	newdata = new Element[num_alloc];
				if (start)
				{
					for (Index i = 0; i < num_elements; i++)
						newdata[i] = start[i];
					delete[] start;
				}
				start = newdata;
			}

	ArrayBody(ArrayBody&) = delete;		// Never copy a body
	ArrayBody& operator=(const ArrayBody& s1) // Assignment operator; ONLY for no-copy bodies
			{
				assert(s1.num_alloc == 0);	// Must not do this if we would make two references to allocated data
				start = s1.start;
				num_elements = s1.num_elements;
				num_alloc = 0;
				return *this;
			}
};
#endif
