Monitor and commands

To start the monitor, run this from the repo root:

idf.py -B build_console -p /dev/cu.usbmodem14101 monitor

- Fallback: if idf.py gives trouble, screen /dev/cu.usbmodem14101 115200 works too. Quit it with Ctrl-A then K.
- Port: use the USB port, not COM, and check the name with ls /dev/cu.usbmodem*.
- Reset trouble: opening the monitor resets the chip. If you see waiting for download, press RESET on its own.
- Quitting the monitor: press Ctrl-].

At the wifi> prompt, type scan and press Enter. The table appears about 2–3 seconds later. The other commands are:

┌─────────┬───────────────────────────────────┐
│ Command │           What it does            │
├─────────┼───────────────────────────────────┤
│ auto 10 │ scan every 10 s (auto 0 stops it) │
├─────────┼───────────────────────────────────┤
│ info    │ heap and stack use                │
├─────────┼───────────────────────────────────┤
│ tasks   │ list all FreeRTOS tasks           │
├─────────┼───────────────────────────────────┤
│ quit    │ stop the scanner thread           │
├─────────┼───────────────────────────────────┤
│ help    │ list the commands                 │
└─────────┴───────────────────────────────────┘

Scan output (the latest run on the board)

15 access points
 SSID                              dBm  Ch Auth      BSSID
 ASUS_9A                           -44   8 WPA2      a0:ad:9f:e7:e3:9a
 WiFi-BVFCT                        -49   6 WPA2/WPA3 d8:33:b7:0d:a3:76
 DIRECT-1O-EPSON-XP-6100 Series    -61   8 WPA2      de:cd:2f:cf:f8:e9
 travinka                          -62   1 WPA/WPA2  94:18:65:37:c5:43
 thomasedward                      -62  11 WPA2      08:9b:f1:7d:31:a5
 travinka                          -63   1 WPA/WPA2  98:de:d0:c4:f1:78
 Telstra725911                     -68   9 WPA2      f4:6b:ef:72:59:17
 WiFi-346A                         -78   4 WPA2      d8:47:32:b5:34:6a
 Singo                             -80   6 WPA2/WPA3 1e:85:94:d5:14:18
 test                              -82   6 WPA2      16:22:3b:4e:c9:88
 H                                 -83   6 WPA2      14:22:3b:4e:c9:89
 Telstra3AD4CB                     -84   6 WPA2      20:b0:01:3a:d4:cb
 FernieFi                          -88   6 WPA2      24:29:34:cc:1a:6d
 NetComm 9923                      -88  11 WPA2/WPA3 d0:db:b7:6b:25:79
 TelstraCD9597                     -88  11 WPA2      a4:91:b1:cd:95:97

The earlier run also saw CHEATH at -37 dBm on channel 11, which was missing this time. Weak networks come and go between scans.

Kconfig setting

I added STRPP_THREAD_LOCAL_SLOTS in components/strpp/Kconfig, with a range of 1–16 and a default of 4. FreeRTOS's own slot count can't be set from another component, so there's a static_assert in thread_local.h. The build now fails if FREERTOS_THREAD_LOCAL_STORAGE_POINTERS is less than the setting plus one. The console build now uses 5, and the board still runs.

What the tasks are (from the "tasks" command)

┌──────────────┬─────────────────────────────────────────────────────────────────────────────────────────────────┐
│     Task     │                                          What it does                                           │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ main         │ runs app_main, which is the console here                                                        │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ IDLE0, IDLE1 │ run when nothing else can, one per core, and keep the watchdog fed                              │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ ipc0, ipc1   │ one per core, so code on one core can ask the other to run something, such as a flash operation │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ esp_timer    │ runs the high-resolution timer callbacks                                                        │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Tmr Svc      │ FreeRTOS's software-timer daemon                                                                │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ sys_evt      │ the default event loop that esp_event handlers run on, including WiFi events                    │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ wifi         │ the WiFi driver's own task, which does the scanning work                                        │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ tcpip        │ the lwIP network stack                                                                          │
├──────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Thread       │ our scanner, named Thread because strpp doesn't set task names yet (a REVISIT in thread.cpp)    │
└──────────────┴─────────────────────────────────────────────────────────────────────────────────────────────────┘

Everything except Thread is ESP-IDF's, and the console build needs the first nine whether or not you scan. The wifi, tcpip and sys_evt tasks appear because the scanner started WiFi.
