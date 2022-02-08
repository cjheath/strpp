/*
 * Memory leak monitoring and reporting for test programs
 *
 * Provides a way for a test suite to look for unexpected memory leaks
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */

void	start_recording_allocations();
void	report_allocations();
int	unfreed_allocation_count();
