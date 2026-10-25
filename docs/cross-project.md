# Changes made from other projects

Artur works on the bike in more than one Claude project. NavExpress is the main one; the
"ZS125GY-13-E55" project (the bike's technical register and the 190cc engine build)
sometimes needs something from the bike unit. Every change made from another project is
logged here, newest first, so whoever works on NavExpress next knows what changed, why, and
what to leave alone.

Each entry: date, which project, the pull request, what changed, why, and what to watch
for when touching that code.

## 2026-10-25 · ZS125GY-13-E55 project · ECU identification probe

**Why.** The engine is going from 125cc to about 190cc, which needs more fuel and a
different ignition map. The ECU is labelled FAI / New Means 85400-1614, which no tuning
tool supports, but its housing matches a Bosch MSE6.0, which commercial tools can read and
reprogram. Asking the ECU for its identification data settles it: a Bosch answers with
"BOSCH", hardware numbers like `0 261 S0x xxx` and software numbers starting `1037`. If it
is a Bosch, the plan is a reflash; if not, a piggyback injector controller.

**What changed.**
- `main/ecu_id.c`, `main/ecu_id.h` (new, pure C, host test `test/host/test_ecu_id.c`):
  the list of identification requests and the text format of the replies.
- `main/obd_poller.c/.h`: new `obd_poller_request_to()`, the same as
  `obd_poller_request()` but sent to a chosen CAN ID (the ECU's physical `0x7E0` for UDS).
  `obd_poller_request()` now calls it with the configured functional ID, so its behaviour
  is unchanged.
- `main/main.c`:
  - new commands `ECU ID` (also `ECUID`) and `ECU PROBE` (`ECUPROBE`). `ECU ID` asks who the
    ECU is; `ECU PROBE` re-reads the security seed and tries the read-only transfer services
    (23 ReadMemoryByAddress, 35 RequestUpload) without a key. Both print every line as
    `ECUID ...` on the serial log and over BLE;
  - commands can now be typed on the USB serial console (UART0 / CH343, 115200 baud), one
    per line; replies to those are printed on the console too. `ble_send_text()` mirrors
    to the console while a serial command runs; the old body is now `ble_notify_text()`.
    Option `CONFIG_SERIAL_COMMANDS` (menuconfig **Serial console**, default on);
  - `struct ble_command` has a `from_serial` flag and `queue_elm_command()` takes it.
- `main/CMakeLists.txt`: `ecu_id.c`, and `esp_driver_uart` in REQUIRES.
- Docs: `docs/ecu-id-protocol.md` (procedure, Portuguese), `docs/brain.md`, this file.
  Host test added to the "Host tests" workflow.

**Leave alone.** The probe is read-only on purpose: no security key, no programming session
(`10 02`), no writes, routines or resets. The test in `test/host/test_ecu_id.c` enforces
this. The session and seed steps only run with the engine stopped. Please keep it that
way unless Artur asks otherwise from the ZS125 project.

**Status.** Host tests pass and the firmware builds with ESP-IDF 6.0. Flashed and run on
the bike on 2026-10-25 (the CH343 showed up as COM9 that day, not COM8): 39 of 42 steps
answered. Serial commands work (`TRIP` replied on the console). The ECU reported calibration
ID `I614_E05+`, supplier software version `8.72.6.0.0.6-100`, no Bosch identifiers, and a
fixed seed `56 78` at security level 01. Analysis lives in the ZS125GY-13-E55 project.

`ECU PROBE` was run twice the same day (plus three key-cycles earlier), all identical:
`10 03` extended session OK (`50 03 00 32 01 F4`); the security seed `27 01` is constant
`56 78` across reads, runs and key-cycles; `23` ReadMemoryByAddress exists and, with a
4-byte-address request, answers NRC 33 securityAccessDenied (service accepted, only gated by
security access — the 2/3-byte requests were malformed on our side, NRC 13); `35`
RequestUpload is NRC 11 serviceNotSupported. So the flash is readable over service `23` once
the key for the fixed seed is known. One serial note for whoever runs the probe from a host
script: open the port with DTR/RTS deasserted, otherwise the CH343 holds the ESP32 in reset
and nothing answers (`idf.py monitor` already does this).
