/*
 * WifiScanner: a Thread that owns the WiFi driver and scans for access points.
 */
#include	<string.h>
#include	<stdlib.h>

#include	<esp_wifi.h>
#include	<esp_netif.h>
#include	<esp_event.h>
#include	<nvs_flash.h>

#include	<strval.h>
#include	<variant.h>

#include	"wifi_scanner.h"

static const int	MAX_APS = 40;
static const ThreadParams	scanner_params = { 8192 };

WifiScanner::WifiScanner(MessageQueue& a_replies)
: Thread(&scanner_params)
, replies(a_replies)
{
	resume();
}

void
WifiScanner::reply(const VariantArray& message)
{
	replies.push(Variant(message));		// Wrapped, or push() would send each element separately
}

void
WifiScanner::reply_error(const char* what, const char* detail)
{
	reply(VariantArray() << "error" << StrVal::format("{1}: {2}", VariantArray() << what << detail));
}

bool
WifiScanner::check(esp_err_t err, const char* what)
{
	if (err == ESP_OK)
		return true;
	reply_error(what, esp_err_to_name(err));
	return false;
}

bool
WifiScanner::start_wifi()
{
	if (!check(nvs_flash_init(), "nvs_flash_init"))
		return false;
	if (!check(esp_netif_init(), "esp_netif_init"))
		return false;

	esp_err_t	err = esp_event_loop_create_default();
	if (err != ESP_ERR_INVALID_STATE && !check(err, "esp_event_loop_create_default"))
		return false;		// ESP_ERR_INVALID_STATE: the loop already exists, which is fine

	if (!esp_netif_create_default_wifi_sta())
	{
		reply_error("esp_netif_create_default_wifi_sta", "failed");
		return false;
	}

	wifi_init_config_t	config = WIFI_INIT_CONFIG_DEFAULT();
	return check(esp_wifi_init(&config), "esp_wifi_init")
		&& check(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode")
		&& check(esp_wifi_start(), "esp_wifi_start");
}

static const char*
auth_name(wifi_auth_mode_t auth)
{
	switch (auth)
	{
	case WIFI_AUTH_OPEN:		return "open";
	case WIFI_AUTH_WEP:		return "WEP";
	case WIFI_AUTH_WPA_PSK:		return "WPA";
	case WIFI_AUTH_WPA2_PSK:	return "WPA2";
	case WIFI_AUTH_WPA_WPA2_PSK:	return "WPA/WPA2";
	case WIFI_AUTH_WPA3_PSK:	return "WPA3";
	case WIFI_AUTH_WPA2_WPA3_PSK:	return "WPA2/WPA3";
	case WIFI_AUTH_OWE:		return "OWE";
	default:			return "other";
	}
}

// An SSID is up to 32 bytes of anything, usually UTF-8; StrVal substitutes for any illegal bytes
static StrVal
ssid_string(const uint8_t* ssid)
{
	return StrVal((const char*)ssid, (StrValIndex)strnlen((const char*)ssid, 32));
}

static StrVal
bssid_string(const uint8_t* b)
{
	return StrVal::format("{1:x02}:{2:x02}:{3:x02}:{4:x02}:{5:x02}:{6:x02}",
		VariantArray() << (int)b[0] << (int)b[1] << (int)b[2] << (int)b[3] << (int)b[4] << (int)b[5]);
}

bool
WifiScanner::scan_into(VariantArray& aps)
{
	wifi_scan_config_t	config;
	memset(&config, 0, sizeof config);	// All channels, active scan, hidden APs not shown
	if (!check(esp_wifi_scan_start(&config, true), "esp_wifi_scan_start"))
		return false;

	uint16_t		found = 0;
	if (!check(esp_wifi_scan_get_ap_num(&found), "esp_wifi_scan_get_ap_num"))
		return false;
	uint16_t		count = found < MAX_APS ? found : MAX_APS;

	wifi_ap_record_t*	records = (wifi_ap_record_t*)calloc(count ? count : 1, sizeof *records);
	if (!records)
	{
		esp_wifi_clear_ap_list();
		reply_error("scan", "out of memory");
		return false;
	}
	if (!check(esp_wifi_scan_get_ap_records(&count, records), "esp_wifi_scan_get_ap_records"))
	{
		free(records);
		return false;
	}

	// The driver returns them strongest-first already, but doesn't promise to
	qsort(records, count, sizeof *records, [](const void* a, const void* b)
		{ return (int)((const wifi_ap_record_t*)b)->rssi - ((const wifi_ap_record_t*)a)->rssi; });

	for (int i = 0; i < count; i++)
		aps << Variant(VariantArray()
			<< ssid_string(records[i].ssid) << (int)records[i].rssi << (int)records[i].primary
			<< auth_name(records[i].authmode) << bssid_string(records[i].bssid));
	free(records);
	return true;
}

// Scan without holding a Window up, then publish the result
bool
WifiScanner::scan_and_publish()
{
	VariantArray	aps;
	if (!scan_into(aps))
		return false;
	scan.update([&](WifiScan& d) { d.last_scan = aps; d.scan_count++; });
	return true;
}

// Handle one request, or the end of the auto interval if request is null. Return -1 to
// carry on, or the exit code of the thread. Data is only changed in scan.update().
int
WifiScanner::handle(const Variant& request, VariantArray& announcement)
{
	if (request.is_null())
	{		// The interval passed with no request
		if (scan_and_publish())
			announcement << "scan" << (int)scan.unguarded().scan_count;
		return -1;
	}

	if (request.type() != Variant::VarArray)
	{
		reply_error("request", "not an array");
		return -1;
	}
	VariantArray	args = request.as_variant_array();
	if (args.length() == 0 || args[0].type() != Variant::String)
	{
		reply_error("request", "no command name");
		return -1;
	}

	StrVal		command = args[0].as_strval();
	if (command == "scan")
	{
		if (scan_and_publish())
			announcement << "scan" << (int)scan.unguarded().scan_count;
	}
	else if (command == "auto")
	{
		if (args.length() != 2 || args[1].type() != Variant::Integer || args[1].as_int() < 0)
			reply_error("auto", "needs a count of milliseconds, or 0");
		else
			scan.update([&](WifiScan& d) { d.auto_ms = args[1].as_int(); });
	}
	else if (command == "quit")
	{
		bool	stopped = check(esp_wifi_stop(), "esp_wifi_stop")
			&& check(esp_wifi_deinit(), "esp_wifi_deinit");
		scan.update([](WifiScan& d) { d.ready = false; });
		announcement << "quit";
		return stopped ? 0 : 1;
	}
	else
		reply_error("request", "unknown command");
	return -1;
}

int
WifiScanner::run()
{
	if (!start_wifi())
		return 1;

	scan.update([](WifiScan& d) { d.ready = true; });
	reply(VariantArray() << "ready");

	for (;;)
	{
		Variant		request = scan.unguarded().auto_ms > 0 ? requests.pop(Milliseconds(scan.unguarded().auto_ms)) : requests.pop();
		VariantArray	announcement;
		int		exit_code = handle(request, announcement);
		if (announcement.length() > 0)
			reply(announcement);
		if (exit_code >= 0)
			return exit_code;
	}
}
