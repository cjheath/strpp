# wifi_scanner

A `Thread` that owns the WiFi driver and scans for access points. No other
thread calls `esp_wifi_*`; you talk to the scanner over `MessageQueue`s.

    MessageQueue    inbox;                       // where replies arrive
    WifiScanner scanner(inbox);           // starts the thread

    scanner.requests.push(Variant(VariantArray() << "scan"));   // wrap it: push(VariantArray) sends each element separately

    Variant reply = inbox.pop();

## Requests

Push a `Variant` holding a `VariantArray` onto `scanner.requests`:

- `["scan"]` - scan now.
- `["auto", ms]` - scan again whenever `ms` milliseconds pass with no
  request. `0` stops it.
- `["quit"]` - stop the WiFi driver and end the thread. You can then `join()`.

## Replies

Each reply is a `VariantArray` whose first element names it:

- `["ready"]` - the driver started. You get this once, first.
- `["scan", n]` - scan number `n` finished and its access points are
  available: see "Reading the access points".
- `["error", text]` - a request or the driver failed. If the driver failed
  to start, the thread then ends.
- `["quit"]` - the thread is about to end.

## Reading the access points

The scanner keeps what it knows in a `WifiScan`, which you read through
a `ReadWindow` on `scanner.scan` rather than receiving it in a message:

    ReadWindow<WifiScan> w(scanner.scan, Milliseconds(1000));
    if (w.holding())
        VariantArray aps = w->last_scan;     // copy it, so you can close the Window

While your Window is open the scanner changes none of this. It scans without
holding a Window up and only waits for yours to close when it has a result to
store, so a Window opens at once unless the scanner is storing one. It has:

- `ready` - the driver is running.
- `auto_ms` - the `auto` interval, or 0.
- `scan_count` - the number of scans completed; the `n` of the `scan` reply.
- `last_scan` - the access points of the latest scan, strongest first. Each
  is `[ssid, rssi, channel, auth, bssid]`: a string, an integer in dBm, an
  integer, one of `open`, `WEP`, `WPA`, `WPA2`, `WPA/WPA2`, `WPA3`,
  `WPA2/WPA3`, `OWE` or `other`, and a string like `aa:bb:cc:dd:ee:ff`.

At most 40 access points are kept.
