# Central node storage (FATFS + microSD)

Binary LogEntry_t logging to a microSD card on the FRDM-MCXN236 central node.

- Board: NXP FRDM-MCXN236
- SDK: MCUXpresso SDK 2026.09.00-pvw2, west workspace (correct this if your version differs)
- Based on the SDK hello_world demo. To build, copy these files over the
  hello_world example folder in your SDK and build with CMake/Ninja.
  The shared LogEntry_t comes from common/log_entry.h in this repo.
- SD card wiring (LPSPI3): CS = P1_17, SCK = P1_13, MOSI = P1_12, MISO = P1_14
- Log files: LOG####.BIN in the card root, 1 MB per file, a new file each
  time logging_init() runs
- Timestamps: fixed date while FF_FS_NORTC = 1 in ffconf.h (RTC driver
  pending, Person A)
