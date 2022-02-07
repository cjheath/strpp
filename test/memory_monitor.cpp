/*
 * Memory leak monitoring and reporting for test programs
 */
#include	<vector>
#include	<stdio.h>
#include	<stdlib.h>

struct	Allocation
{
	void*		location;
	size_t		size;
	bool		freed;
};

std::vector<Allocation>	allocations;
bool			recording_allocations = false;

void	start_recording_allocations()
{
	allocations.clear();
	recording_allocations = true;
}

void	report_allocations()
{
	size_t	total_growth = 0;
	int	count_growth = 0;
	for (auto iter = allocations.begin(); iter != allocations.end(); iter++)
	{
		if (iter->freed)
			continue;
		printf("\tAllocated %lu at %p, %s\n", iter->size, iter->location, iter->freed ? "now freed" : "still allocated");
		if (!iter->freed)
		{
			total_growth += iter->size;
			count_growth++;
		}
	}
	printf("Growth is %lu bytes in %d allocations\n", total_growth, count_growth);
}

int	unfreed_allocation_count()
{
	int	count_growth = 0;
	for (auto iter = allocations.begin(); iter != allocations.end(); iter++)
		if (!iter->freed)
			count_growth++;
	return count_growth;
}

void*	operator new(std::size_t n) throw(std::bad_alloc)
{
	void*	vp = malloc(n);
	if (recording_allocations)
	{
		recording_allocations = false;
		allocations.push_back({
			.location = vp,
			.size = n,
			.freed = false
		});
		recording_allocations = true;
	}
	return vp;
}

void operator delete(void * p) throw()
{
	auto iter = allocations.begin();
	for (; iter != allocations.end(); iter++)
	{
		if (iter->location == p && !iter->freed)
		{
			iter->freed = true;
			break;
		}
	}
	if (iter == allocations.end())
		;	// Memory freed that was not in recorded allocations; perhaps allocated before recording started?

	free(p);
}
