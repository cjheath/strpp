# freertos_console

A test driver for strpp on a real FreeRTOS: an ESP-IDF project for an ESP32-S3,
with a serial console. It uses no LCD and no LVGL. A `WifiScanner` thread (in
`components/wifi_scanner/`) does the WiFi scanning, the main thread reads the
console, and the two talk through a `MessageQueue` and a `Transactional`.

With the monitor on (`CONFIG_STRPP_MONITOR=y`, which `sdkconfig.defaults` sets)
the console also has `monitor`, `deadlock`, `stall`, `errors`, `jam`,
`ballast <KB>` and `reap`, which start threads that misbehave for the monitor to
find. Type `help` for the list.

## Build, flash and run

Activate ESP-IDF 6.1, then from this directory:

    idf.py build flash monitor -p /dev/cu.usbmodem14101

- Use the USB-Serial-JTAG port, which the board labels USB. The device name
  changes: `ls /dev/cu.usbmodem*`.
- Only one program can hold the port: quit your own `idf.py monitor` (Ctrl-])
  before another one starts.
- After flashing, the chip can stay in download mode: press RESET on its own.
- Stdio is used here for the console, because strpp has no terminal I/O yet.

strpp is the component two directories up; this project needs no copy of it.
`make freertos_check` in the strpp directory checks the FreeRTOS branches of
the library without hardware; this project is where they run.
