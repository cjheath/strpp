/*
 * Memory leak monitoring and reporting for test programs
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<vector>
#include	<cstdio>
#include	<cstdlib>

#if	!defined(MEMORY_GUARD)
#define MEMORY_GUARD	4
#endif
#define	GUARD_VALUE	0xA5

void check_arena();
void memory_error();			// Breakpoint here to stop on memory errors

static	void*	memory_focus = 0;	// Set this in the debugger, and a new or delete of this address will call memory_error
static	long	memory_count;		// Increments with every memory operation (new or delete)
static	long	memory_limit = -1;	// Set this to a memory_error at a specific memory_count
static	long	memory_recursion;	// Don't do any magic on recursive calls

struct	Allocation
{
	char*		location;
	size_t		size;
	bool		freed;
	bool		baseline;	// True if this allocation was before we started recording allocations
	long		alloc_num;
};

std::vector<Allocation>	allocations;

void	start_recording_allocations()
{
	for (auto iter = allocations.begin(); iter != allocations.end(); iter++)
		iter->baseline = true;
}

// How many allocations since start_recording_allocations remain allocated?
int	allocation_growth_count()
{
	int	count_growth = 0;
	for (auto iter = allocations.begin(); iter != allocations.end(); iter++)
		if (!iter->freed && !iter->baseline)
			count_growth++;
	return count_growth;
}

void	report_allocation_growth()
{
	check_arena();
	size_t	total_growth = 0;
	int	count_growth = 0;
	for (auto iter = allocations.begin(); iter != allocations.end(); iter++)
	{
		if (iter->freed)
			continue;
		printf("\tAllocated %lu at %p (alloc %ld), %s\n", iter->size, iter->location, iter->alloc_num, iter->freed ? "now freed" : "still allocated");
		if (iter->baseline)
			continue;
		total_growth += iter->size;
		count_growth++;
	}
	printf("Allocation growth is %lu bytes in %d allocations\n", total_growth, count_growth);
}

void memory_error()
{
	;	// Breakpoint here to stop at memory errors
}

long	last_alloc_num()
{
	return memory_count;
}

void check_allocation(const Allocation& a, const char* p)
{
	for (int i = 0; i < MEMORY_GUARD; i++)
	{
		if ((p[i-MEMORY_GUARD]&0xFF) != GUARD_VALUE)
		{
			printf("MEMORY: Guard damaged before start at %p (alloc %ld)\n", p, a.alloc_num);
			memory_error();
			break;
		}
		if ((p[a.size+i]&0xFF) != GUARD_VALUE)
		{
			printf("MEMORY: Guard damaged after end at %p (alloc %ld)\n", p, a.alloc_num);
			memory_error();
			break;
		}
	}
}

void check_arena()
{
	if (MEMORY_GUARD != 0)
		for (auto iter = allocations.begin(); iter != allocations.end(); iter++)
			if (!iter->freed)
				check_allocation(*iter, iter->location);
}

void*	operator new(std::size_t n)
{
	char*	cp = (char*)malloc(n + 2*MEMORY_GUARD);
	cp += MEMORY_GUARD;
	if (memory_recursion++ == 0)
	{
		auto iter = allocations.begin();
		Allocation	a {
				.location = cp,
				.size = n,
				.freed = false,
				.baseline = false,
				.alloc_num = ++memory_count
			};
		for (; iter != allocations.end(); iter++)
		{
			if (iter->location != cp)
				continue;
			*iter = a;
			break;
		}
		if (iter == allocations.end())
			allocations.push_back(a);

		for (int i = 0; i < MEMORY_GUARD; i++)
			cp[i-MEMORY_GUARD] = cp[n+i] = GUARD_VALUE;
		if (memory_count == memory_limit
		 || memory_focus == cp)
			memory_error();
	}
	memory_recursion--;
	return cp;
}

void operator delete(void * vp) throw()
{
	if (memory_recursion++ == 0)
	{
		if (++memory_count == memory_limit
		 || memory_focus == (char*)vp)
			memory_error();
		auto iter = allocations.begin();
		for (; iter != allocations.end(); iter++)
		{
			if (iter->location != (char*)vp)
				continue;
			if (iter->freed)
			{
				printf("MEMORY: Freeing free memory at %p (alloc %ld)\n", vp, iter->alloc_num);
				memory_error();
				break;
			}

			check_allocation(*iter, (const char*)vp);
			iter->freed = true;
			break;
		}
		if (iter == allocations.end()
		 && vp != &allocations[0])	// It's not the allocations array itself
		{
			// Memory freed that was not in recorded allocations; perhaps allocated before recording started?
			printf("MEMORY: Freeing non-allocated memory at %p\n", vp);
			memory_error();
		}
	}
	memory_recursion--;
	free((char*)vp-MEMORY_GUARD);
}
