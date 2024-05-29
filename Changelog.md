# Version 1.1

## New and changed features

- The digipeating method is now configurable via the `APRS Config`→`Advanced`→`Digipeating` menu item. The following settings are available:
  - `off`: Digipeating is not requested.
  - `Dest. Call`: Destination call digipeating. This was used up to version 1.0.
  - `WIDEn-n`: The “classic” digipeating mode, requested by putting `WIDE1-1` in the repeater path.

Please note: due to the internal settings storage method, the digipeating will be `off` after the upgrade. Make sure you check this new setting if you want your packets to be digipeated.

# Version 1.0

## New and changed features

- Changed APRS TOCALL to _APLETK_, which is [officially
  registered](https://github.com/aprsorg/aprs-deviceid/blob/3ff2b5c907d8b85a83f9387195f282d14a6f8fb2/tocalls.yaml#L683)
  for this firmware.
- Frequently requested and finally introduced: It is now possible to start RX and/or TX right after firmware startup (device reset). All combinations of RX and TX can be selected through the `APRS Config`→`Advanced`→`Startup state` menu item.
- Allow to shut down the firmware via the menu. This stops all activity (including Bluetooth LE!), clears the display, switches off the peripherals as far as possible and puts the SoC into the lowest power mode. It is only possible to wake the SoC with a reset in this state. Note that even though it is the lowest possible power mode, current is still drawn from the battery. However, it still has some applications:
  - If you use your T-Echo periodically but not very often (once every few weeks), you can put it into shutdown mode to reduce the standby consumption.
  - When you plan not to use the T-Echo for a long time, you can activate this mode to clear the display before removing the battery. This avoids potential display burn-in effects.
  - Also, this mode can be considered the “Airplane mode”, as even BLE is disabled.
- To fit the new shutdown menu entry, the TX power setting was moved to the APRS menu.
- Added a new BLE interface which allows to read and write all settings that are stored in the internal flash.
  - Two characteristics were added to the APRS service: one to write or select a setting, the other to read back the current value of the setting (which may be different to the attempted write).
  - To prevent modification of settings without physical access to the device, the new characteristics require MITM access level. That means that the BLE client must have been paired using the passkey that is shown on the T-Echo’s display.
  - Settings are accessible on the lowest possible level through this interface (warning: this may result in unexpected behaviour if conflicting or wrong values are written). Make sure you read the documentation!
- The `ble_client.py` script has been massively extended to support all internal settings through the new interface.
- The RF frequency can now be changed through the new settings interface. This allows the firmware to be used in the UK, for example, where LoRa APRS runs on a different frequency. The default frequency of 433.775 MHz is unchanged.


## Fixed Bugs

- The -9 dBm entry was cut off at the bottom of the screen. This was fixed by
  removing the “Cancel” entry in the menu. The current setting is now
  preselected in the menu, so you can „cancel“ the menu by simply confirming
  the preselected entry.
- If packets were received before a GNSS timestamp is available, those packets
  were displayed with an age of >19000 days once the time became valid. Now the
  timestamps are updated such that the packet age is always correct. Thanks to
  Chris DL5CV for providing this patch!

# Version 0.9

This release vastly improves the behaviour of this firmware on-air.

## Improvements to the APRS Implementation

Special thanks in this area go to Thomas DL9SAU who provided provided ideas,
suggestions and code to improve the APRS protocol implementation in this
firmware.

The following changes have been made:

- Handle further common frame types (such as positionless weather reports) as
  valid frames and show their info field in the RX comment area.
- Keep the last known position of a station if a positionless frame was
  received from that station.
- Airtime reduction:
  - Use “destination call digipeating”. Instead of the usual "WIDE1-1" in the
    path, SSID "-1" is set in the destination call field. This still allows
    digipeating while saving 6 bytes in every APRS transmission.
  - No digipeating is used for weather reports.
  - Do no longer transmit the APRS comment in every frame. The comment is
    transmitted at least once per hour or in every 10th position packet,
    whatever happens first. However, the minimum time between two comment
    transmissions is 10 minutes.
  - Position updates are now forced every 30 minutes (previously 15 minutes) to
    reduce the airtime impact of non-moving stations.
- Use human readable version of the `!DAO!` field (format `!W42!` instead of
  `!wnn!`). As this firmware internally calculates in 32-bit floating point,
  position resolution is limited to 2.38m anyway. The new DAO variant’s
  resolution is 1.8 m (or better), so no precision is lost by this change.
- Weather data is now compliant to the APRS specification and always includes
  the required `c`, `s` and `g` fields.
- Send weather data in positionless weather reports. This completely separates
  weather data from position data and the previous toggling of the symbol
  between the selected one and the WX symbol is no longer necessary.
  Positionless weather report are inserted between the regular position updates
  every 5 minutes, if enabled.
- Improved robustness during frame generation. If a field does not fit into the
  frame memory, it is dropped completely instead of truncating it. This
  prevents parser errors at the receiver.

## Other Improvements

- The compass displays now show a nice arrow instead of a simple line. This
  should make the direction easier to see.

## Fixed Bugs

- Fixed APRS RX history handling: while the RX list was not full, newly
  received packets were always appended to the list even if the station was
  heard before. This caused a single station to fill up the entire list.


# Version 0.8.1

This is primarily a bugfix release.

## New and changed features

- Decreased the BME280 readout and display refresh interval to 15 seconds.
  - Readout/Refresh only happen if power is already on for these modules, for
    example while the tracker is running.
  - Auto-refresh happens only once per hour in standby.

## Fixed bugs

- Fixed a crash/hang on startup (during settings storage initialization) that
  could happen when this firmware is first installed.
- Give GNSS module more time after a reset. That ensures that it can be
  configured properly over UART and fixes the bug that GNSS details were not
  shown on some devices.

## Documentation

There is some work-in-progress documentation now, describing the [features of
the firmware](doc/features.adoc) in more detail than the [README.md](README.md)
could. Also, there is a [user guide](doc/manual.adoc) describing how to operate
the T-Echo with this firmware.

The documentation is written in [AsciiDoc](https://asciidoc.org).

# Version 0.8

## New and changed features

- Implement quick RX/TX toggling by holding the touch button and shortly
  pressing the lower-left button.  The button combination enables or disables
  receiver and tracker in the following order:
  - both RX and TX off
  - RX on, TX off
  - both RX and TX on
  - TX on, RX off
  - both RX and TX off again

## Fixed bugs

- Fixed small inconsistencies in the button handling module.

## Thanks

- to Tom DL5NEN for suggesting the quick RX/TX switching method.


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
