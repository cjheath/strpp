/*
 * Memory leak monitoring and reporting for test programs
 *
 * Provides a way for a test suite to look for unexpected memory leaks
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */

void	start_recording_allocations();
void	report_allocation_growth();
int	allocation_growth_count();
long	last_alloc_num();	// Return the counter of the last allocation operation, for tracing leaks

void	check_arena();
