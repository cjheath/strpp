/*
 * The error buffer's global API.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<errbuf.h>

/*
 * One slot for the process, claimed as this object is constructed. A namespace
 * scope static rather than a function-local one, which would need a
 * thread-safe-initialisation guard on every call - and FreeRTOS builds may not
 * have the locks that guard wants. Nothing may report an error before static
 * initialisation has run.
 */
static ThreadLocal<ErrBuf>	the_error_buffer;

ThreadLocal<ErrBuf>&
error_buffer()
{
	return the_error_buffer;
}

ErrNum
Error(ErrNum err, const char* default_text, VariantArray params)
{
	error_buffer().get()->report(err, default_text, params);
	return err;
}
