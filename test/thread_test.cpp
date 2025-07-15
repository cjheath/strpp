#include	<cstdio>

#include	<lockfree.h>
#include	<thread.h>

#define	FANOUT	25	// This many primary threads will each create this many again. total of N*(N+1)

class	HelloThread
	: public Thread
{
	int	count;
public:
	HelloThread(int a_count = 0)
	: count(a_count)
	{ resume(); }

	int	run()
	{
		printf(
			"Hello, world, from thread 0x%llX\n",
			(long long)thread_id
		);
		yield();
		for (int i = 0; i < count; i++)
			(void)new HelloThread();
		printf(
			"Goodbye, cruel world, from thread 0x%llX\n",
			(long long)thread_id
		);
		return 0;
	}
};

int
main(int argc, const char** argv)
{
	// Ensure that stdout gets flushed:
        setvbuf(stdout, 0, _IONBF, 0);

	printf(
		"ProcessID is %d, Main thread is 0x%llX\n",
		Thread::currentProcessId(),
		(long long)Thread::main()->id()
	);

	for (int i = 0; i < FANOUT; i++)
                (void)new HelloThread(FANOUT);

        Thread*        ended = 0;
        while ((ended = Thread::joinAny()) != 0)
        {
		// REVISIT: Show any outstanding errors on this thread
                delete ended;
                printf("Ended %p\n", ended);
        }

	// REVISIT: Show any outstanding errors on the main program
	return 0;
}
