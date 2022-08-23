# In development

## New and changed features

- Show own heading as dashed line in the RX direction diagram for comparison.

## Fixed bugs

- Last character of APRS comment was missing in the parsed packet view.
- fixed crash on startup after initial installation. This happened if the flash
  area where the settings are stored was not clean (erased), i.e. contained data
  from a different firmware. In this case, the settings area is now erased and
  the flash storage is re-initialized.

# Version 0.4

## New and changed features

- Support for sending and decoding more APRS packet formats:
  - Compressed data format as specified in APRS 1.0. Encodes high-precision
    latitude, longitude, altitude and symbol in only 13 characters!
  - `!DAO!` extension as specified in APRS 1.2 for enhanced location precision
    in uncompressed packets.
- The following fields in the APRS info field are now optional:
  - Altitude (always on in compressed packets)
  - `!DAO!` (always off in compressed packets)
  - Packet counter
- Reorganized the menu:
  - new “APRS Config” submenu in main menu that allows to configure the options
    listed above.
  - moved the symbol selection menu to the new “APRS Config” menu.
- Tuned the tracker transmission conditions:
  - Trigger transmission at 30° heading change (previously 45°).
  - Force transmission 15 minutes after the last one (previously 3 minutes).

## Fixed bugs

- correctly display negative SNRs for received packets (these were previously
  shown as +32 to +65 dB SNR).


# Version 0.3

- Decode and count visible and tracked GNSS satellites
- Reorganized the status bar at the top of the screen
  - Removed precise battery voltage
  - Battery graph improved
  - Show GNSS status
    - dashed border = GNSS disabled
    - solid border, white background = GNSS active, but no valid position available
    - solid border, black background = Position valid
- Increased fast advertising timeout to three minutes again
  - Allows faster reconnects after the BLE connection was lost
- Fixed an issue where the incorrect symbol code was provided via BLE
- Added a simple BLE client for the PC written in Python


# Version 0.2

Show the source call sign in the BLE device name to help distinguish between
different T-Echos in the same location.


# Version 0.1

Initial public release.

Released on the occasion of the 2022 B26 Field Day.
