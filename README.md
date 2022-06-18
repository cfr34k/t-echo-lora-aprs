# LoRa-APRS firmware for Lilygo T-Echo

This is a custom firmware for the T-Echo devices made by Lilygo.

With this firmware the T-Echo becomes a LoRa-APRS tracker. The LoRa
transmissions are compatible with the popular
[LoRa-APRS-iGate](https://github.com/lora-aprs/LoRa_APRS_iGate) firmware for
some ESP32-based devices usually used as iGates.

One particular focus of this firmware is to use the low-power capabilites of
the T-Echo. With the standard 800 mAh battery, it achieves more than 10 hours
of active tracking, multiple days of lora reception and 1 to 4 months of
standby (reachable via BLE; standby time depends on hardware configuration).

## Features

- Ultra low power usage:
  - At any point, only the necessary modules are powered.
  - nRF52840 is in deep sleep most of the time.
  - Achieves down to 100 μA standby current with full BLE connectivity
    (depending on hardware assembly variant).
- Tracker/transmitter:
  - Smart beacon: transmission times depend on movement.
  - 20 dBm TX power.
- Receiver:
  - Tries to decode received packets and
  - Displays remote station call sign, location, distance and direction on success.
  - Undecodable packets are displayed as raw text.
- E-Paper display:
  - Support for partial refresh (no flickering; 0.3 s refresh time).
  - Force full refresh (with flickering) every 1 hour to remove artifacts and prevent burn-in.
- Bluetooth Low Energy:
  - Configure source call sign, APRS symbol, and comment.
  - Notification support for received APRS messages.

## Disclaimer

I provide this firmware in the hope that it will be useful. However I can not
guarantee that it will work on your device.

**It may even break your device** if you have a different hardware version with a
different pin assignment or power distribution network or other
LoRa/GNSS/display modules. You are using this on your own risk.

**Before you try this firmware, make sure that the pinout in `config/pinout.h`
matches your device!**

## Hardware support

This firmware should support T-Echos with the following components:

- BLE SoC nRF52840
- ePaper display GDEH0154D67 with controller SSD1681
- GNSS module L76K (other NMEA-via-UART capable modules should also work)
- LoRa transceiver SX1262

## Usage

The device operation concept is still a bit rudimentary. The hardware buttons
have the following functions:

- Top push button: hard reset.
- Bottom push button:
  - Short press: switch to the next screen.
  - Long press: toggle transmitter (tracking) on/off.
- Touch button:
  - Short touch: enable display backlight for 3 seconds.
  - Long touch: toggle receiver on/off.

## Building the firmware

Before you can compile the firmware it needs some basic configuration. Copy
`src/config.h.template` to `src/config.h` and edit it. At the very least set
your amateur radio call sign in `APRS_SOURCE`.

To compile the firmware you need to set up the toolchain for Nordic
Semiconductor’s nRF5 SDK. See [the SDK
documentation](https://infocenter.nordicsemi.com/topic/struct_sdk/struct/sdk_nrf5_latest.html)
for setup instructions. Only compilation via GCC is supported. If you can build
any example from the SDK (located in `nrf5-sdk/examples/`) you should be good
to go.

To compile the firmware, simply run

```sh
make
```

## Flashing the firmware

This firmware is compatible with the [T-Echo’s preinstalled
bootloader](https://github.com/Xinyuan-LilyGO/T-Echo/tree/main/bootloader), so
you can simply install it like an regular firmware update (tested with the
Meshtastic version, not sure about the SoftRF version).

Short reminder: to invoke the bootloader, double-press the reset button and
connect the T-Echo to your PC via USB-C. It should appear as a mass-storage
device called `TECHOBOOT`.

### Initial installation

**Back up your current firmware before installing this firmware.** To do so,
copy the `CURRENT.UF2` from the T-Echo’s mass storage device `TECHOBOOT` to
your PC.

When you first install the LoRa-APRS firmware, you should also install the
correct SoftDevice (Bluetooth stack). Therefore run `make uf2_sd` which creates
`_build/nrf52840_xxaa_with_sd.uf2` which contains both the SoftDevice and the
regular firmware build. Copy `_build/nrf52840_xxaa_with_sd.uf2` to the
`TECHOBOOT`. When the copy operation is complete, the device should disconnect,
reset, and boot the LoRa-APRS firmware.

### Normal updates

If the SoftDevice is unchanged, only the firmware part needs to be updated. To
do so, run `make uf2` which generates `_build/nrf52840_xxaa.uf2`. Copy that
file to `TECHOBOOT` and you are done.
