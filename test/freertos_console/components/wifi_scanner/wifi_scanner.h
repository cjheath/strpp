#if !defined(WIFI_SCANNER_H)
#define WIFI_SCANNER_H
/*
 * WifiScanner: a Thread that owns the WiFi driver and scans for access points.
 * See ReadMe.md for the messages it accepts and sends.
 */
#include	<esp_err.h>
#include	<thread.h>
#include	<msgqueue.h>
#include	<transactional.h>

// What a ReadWindow<WifiScan> shows
struct	WifiScan
{
	WifiScan() : ready(false), auto_ms(0), scan_count(0) {}
	bool		ready;		// The WiFi driver is running
	long		auto_ms;	// Scan again after this long with no request; 0 = never
	unsigned	scan_count;	// Scans completed
	VariantArray	last_scan;	// The access points of the latest, see ReadMe.md
};

class	WifiScanner
: public Thread
{
public:
	// Starts the thread; it sends every reply to "replies", which must outlive it
	WifiScanner(MessageQueue& replies);

	int		run();

	MessageQueue		requests;	// Push a Variant(VariantArray) here, see ReadMe.md
	Transactional<WifiScan>	scan;	// What it knows; read it through a ReadWindow

private:
	MessageQueue&		replies;

	bool		start_wifi();
	bool		check(esp_err_t err, const char* what);
	bool		scan_into(VariantArray& aps);	// Append the access points found to aps; false if it failed
	bool		scan_and_publish();	// scan_into(), then replace the last scan
	int		handle(const Variant& request, VariantArray& announcement);
	void		reply(const VariantArray& message);
	void		reply_error(const char* what, const char* detail);
};

#endif	// WIFI_SCANNER_H
