#if !defined(ARRAY_H)
#define ARRAY_H
/*
 * Array slices:
 * - All access to an array Body is via a lightweight Slice.
 * - Slices should be passed by copying (cheap!) not by pointers or references.
 * - A slice is mutable, but does not affect the base array or its other slices (by-value semantics)
 * - Slices share Arrays with thread-safety and garbage collection using atomic reference counting
 *
 * A shared array Body cannot be mutated, it's always copied first to ensure it has only one Slice.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<stdlib.h>
#include	<stdint.h>
#include	<functional>

#include	<refcount.h>

// Define a type for the element position (offset) within an array or slice
#if	defined	ARRAY_MAX_65K
typedef	uint16_t	ArrayIndex;	// Small index, for embedded use
#else
typedef	uint32_t	ArrayIndex;
#endif

template<typename E, typename I = ArrayIndex> class	Array
{
public:
	class	Body;
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
	Element&	operator[](int elem_num) { return body ? body->data()[offset+elem_num] : *(Element*)0; }
	const Element&	operator[](int elem_num) const { return body ? body->data()[offset+elem_num] : *(Element*)0; }
	const Element*	asElements() const { return body ? body->data()+offset : 0; }		// The caller must ensure to enforce bounds

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
					if (operator[](i) != comparand[i])
						return false;
				return true;
			}
	inline bool	operator!=(const Array& comparand) const { return !(*this == comparand); }

#if 0	// Not yet implemented
	// Linear search for an element using a match function
	int		find(const std::function<bool(const Element* e)> match, int after = -1) const
			{
				Index		n = after+1;		// First Index we'll look at
				const Element*	up = &body[n];
				for (up < (body ? body->end() : 0); n < num_elements; n++, up++)
					if (match(up))
						return n;		// Found at n

				return -1;				// Not found
			}
	int		rfind(const std::function<bool(const Element* e)> match, int before = -1) const
			{
				Index		n = before == -1 ? num_elements-1 : before-1;		// First Index we'll look at
				const Element*	bp = &body[n];
				for (bp > (body ? body->start() : 0); n >= 0; n--, bp--)
					if (match(bp))
						return n;		// Found at n

				return -1;				// Not found
			}
#endif

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

	// Following methods usually mutate the current slice so must Unshare (copy) it.

	void		remove(Index at, int len = -1)			// Delete a section from the middle
			{
				if (len == -1)
					len = num_elements-at;
				if (at == num_elements)
					return;
				assert(num_elements-len >= at);		// Care with unsigned arithmetic

				Unshare();
				body->remove(at, len);
				num_elements -= len;
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
				Body	b(&addend, false, 1, 0);	// No-copy temporary wrapper around the addend
				body->insert(num_elements, Array(&b));
				num_elements++;
				return *this;
			}
	void		insert(Index pos, const Array& addend)
			{
				// Handle the rare but important case of extending a slice with a contiguous slice of the same body
				if (pos == num_elements			// Appending at the end
				 && (Body*)body == (Body*)addend.body	// From the same body
				 && offset+pos == addend.offset)	// And addend starts where we end
				{
					num_elements += addend.length();
					return;
				}

				Unshare(addend.length());
				body->insert(pos, addend);
				num_elements += addend.length();
			}
	void		append(const Array& addend)
			{ insert(num_elements, addend); }
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
	class	Body
	: public RefCounted			// This object will be deleted when the ref_count decrements to zero
	{
	public:
		using		Element = E;
		using		Index = I;

		~Body()
				{
					if (start && num_alloc > 0)
						delete[] start;
				}
		Body()
				: start(0), num_elements(0), num_alloc(0) { }
		Body(const Element* data, bool copy, Index length, Index allocate = 0)
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

		bool		noCopy() const { return num_alloc == 0; }	// This body or its data are transient

		Element&	operator[](int elem_num) const { return start[elem_num]; }

		inline Index	length() const { return num_elements; }
		Element*	data() const { return start; }			// fast access
		Element*	end() { return start+num_elements; }		// ptr to the trailing NUL (if any)

		// Mutating methods. Must only be called when refcount <= 1 (i.e., unshared)
		void		insert(Index pos, const Array& addend)	// Insert a substring
				{
					assert(ref_count <= 1);
					resize(num_elements + addend.length());

					if (num_elements > pos)		// Move data up
						for (Index i = num_elements+pos-1; i >= pos+addend.length(); i--)
							start[i] = start[i-addend.length()];
					for (Index i = 0; i < addend.length(); i++)
						start[pos+i] = addend.asElements()[i];
					num_elements += addend.length();
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
		 * To quit, leaving the remainder untransformed, return without advancing ep
		 * (but a returned Array will still be inserted).
		 * None of the returned Arrays will be retained, so they can use static Bodys (or the same Body)
		 */
		void		transform(const std::function<Array(const Array*& cp, const Array* ep)> xform, int after = -1);
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

		Body(Body&) = delete;		// Never copy a body
		Body& operator=(const Body& s1) // Assignment operator; ONLY for no-copy bodies
				{
					assert(s1.num_alloc == 0);	// Must not do this if we would make two references to allocated data
					start = s1.start;
					num_elements = s1.num_elements;
					num_alloc = 0;
					return *this;
				}
	};

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
#endif
