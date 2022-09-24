# Version 0.7

## New and changed features

- Raise the BLE security level:
  - The device now supports BLE 4.0 passkey pairing: The T-Echo shows a passkey
    that you have to type at your phone/laptop. A successful passkey pairing
    ensures man-in-the-middle protection.
  - Passkey pairing is now required to write to any characteristic. Thus (for
    example) the call sign can only be changed if you can see the T-Echo’s
    passkey.
- Allow to cold-restart the GNSS module via the menu.
- Added support for the BME280 environment sensor:
  - detect the sensor on firmware startup (may not be available)
  - read temperature, humidity and pressure values every minute (at most)
  - as the sensor is connected to the peripheral power which also supplies the
    GNSS module, a measurement is only started if peripheral power is already on.
  - measurement is triggered
    - by a timer every 60 seconds
    - by any button press
- Weather reports based on the BME280’s values can be transmitted via APRS:
  - optional, configurable via the advanced APRS configuration menu.
  - Weather reports are transmitted when a transmission is triggered by the
    tracker, i.e. no additional packets are sent. They include the latest
    BME280 readings available at that time.
  - In weather reports, the symbol code is forced to `/_`. This is required by
    the APRS specification. All other data is included as usual.
- Changes in the menu:
  - New submenu _GNSS Utilities_ in main menu.
    - Renamed the _GNSS Warmup_ to _Keep GNSS powered_ and moved it to the new
      _GNSS Utilities_ submenu.
    - Added a new menu entry _Cold restart_ to the _GNSS Utilities_ submenu.
  - New entry _Weather report_ in _APRS Config → Advanced_ which allows to
    enable transmission of weather reports.

## Fixed bugs

- #4 _No GNSS satellite status displayed on some devices_ should be fixed with
  this release. Now the [CASIC
  command](https://espruino.microco.sm/api/v1/files/68b597874e0a617692ebebc6a878032f76b34272.pdf)
  to enable all (and only) the necessary NMEA sentences is sent to the module
  at each firmware boot.


# Version 0.6

## New and changed features

- Track the current wall clock time (in UTC) and synchronize it to GNSS when enabled.
- Added a clock screen showing the current time and date.
- Added decoder overview screen showing the receiver history, including time since
  the last error.
  - For each decoded station, the time since reception and call sign are always shown.
  - If own location is available, distance and course are also displayed.
- Overview screen is a mini menu where one station can be selected for the details screen.
- Merged decoded packet and raw packet data screens into one “packet details”
  screen. The contents depend on the entry selected in the overview screen:
  - For decoded packets, the details screen corresponds to the previous decoded
    packet screen, with the following changes:
    - Moved distance to the received station below the graphical course representation.
    - Removed numeric course to the station.
    - Show two lines of APRS comment.
    - Show received signal properties (RSSI, SNR, SignalRSSI).
  - For packets that cannot be decoded, a screen similar to the previous raw
    data screen is shown:
    - Show error message, raw packet and signal properties on one screen.
- Improved the tracker status screen:
  - Heading is now shown similar to the course on the packet detail screen.
  - Speed is in km/h and moved below the heading display.
  - DOP was removed (still available from the GNSS screen).
- Pressing the touch button now always refreshes the current screen.


# Version 0.5

## New and changed features

- Show own heading as dotted line in the RX direction diagram for comparison
  with the course to the received station.
- Add option to send the battery voltage in the APRS comment field.
- Changes in the menu:
  - Introduced “Advanced” submenu below the “APRS Config” submenu which
    includes the new Battery Voltage option.
  - Moved the Frame Counter option to the new “Advanced” submenu.

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
