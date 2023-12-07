#!/usr/bin/env python3

import asyncio
import struct
from bleak import BleakScanner, BleakClient

UUID_CHAR_SOURCE_CALL = '00000101-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_APRS_COMMENT = '00000102-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_APRS_SYMBOL = '00000103-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_RX_MESSAGE = '00000104-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_SETTING_WRITE_SELECT = '00000110-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_SETTING_READ = '00000111-b493-bb5d-2a6a-4682945c9e00'

SETTINGS_IDS = {
        'SOURCE_CALL':      0x0001,
        'SYMBOL_CODE':      0x0002,
        'COMMENT':          0x0003,
        'LORA_POWER':       0x0004,
        'APRS_FLAGS':       0x0005,
        'LAST_BLE_SYMBOL':  0x0006,
    }

LORA_POWERS_DBM = [22, 20, 17, 14, 10, 0, -9]

async def get_adv_setting_data(client, setting_id):
    await client.write_gatt_char(UUID_CHAR_SETTING_WRITE_SELECT, struct.pack('B', setting_id))
    raw_data = await client.read_gatt_char(UUID_CHAR_SETTING_READ)

    if raw_data[0] & 0x80 != 0:
        print(f"Error: could not read setting #{setting_id}.")
        return None

    if raw_data[0] & 0x7F != setting_id:
        print(f"Error: device returned wrong setting ID. Requested #{setting_id}, received #{raw_data[0] & 0x7F}.")
        return None

    return raw_data[1:]


def format_setting(setting_name, data):
    if setting_name in ['SOURCE_CALL', 'COMMENT', 'SYMBOL_CODE', 'LAST_BLE_SYMBOL']:
        null_idx = data.index(0)
        return data[:null_idx].decode('utf-8')
    elif setting_name == 'LORA_POWER':
        index = data[0]
        return f"{LORA_POWERS_DBM[index]} dBm"
    elif setting_name == 'APRS_FLAGS':
        decoded, = struct.unpack('<I', data)
        return f"0x{decoded:08x}"
    else:
        return f"{data}"


async def advanced_config(client):
    print("\n### Advanced settings menu ###")
    print("\nCurrent configuration:\n")

    for name, setting_id in SETTINGS_IDS.items():
        data = await get_adv_setting_data(client, setting_id)
        formatted = format_setting(name, data)
        print(f"#{setting_id:03d} {name:15s} -> {formatted}")

    print()


async def main():
    print("Scanning for 5 seconds...")

    devices = await BleakScanner.discover()

    techos = []
    for d in devices:
        if d.name[:6] == "T-Echo":
            techos.append(d)

    print("Found the following T-Echos:")
    for i in range(len(techos)):
        d = techos[i]
        print(f"{i} = [{d.address}] {d.name}")

    idx = int(input("Type the entry number to connect: "))

    print(f"You selected #{idx}: {techos[idx]}")

    print("Connecting...")

    techo = techos[idx]
    is_paired = techo.details['props']['Paired']

    client = BleakClient(techo)

    try:
        await client.connect()

        print("Connected!\n")

        if not is_paired:
            print("!!! This device is not paired with your PC!")
            print("If you want to change settings, you must pair the device.")
            print("Please note: pairing using this tool is experimental and may")
            print("not work. Please also try to pair using your operating")
            print("system’s facilities.\n")

            while True:
                answer = input("Do you want to pair now [y/n]? ")
                if answer[0] not in "yn":
                    print("Answer not understood. Please type y or n.")
                    continue
                elif answer[0] == 'y':
                    is_paired = await client.pair()

                break

        while True:
            print("\n### Main menu ###")
            print("\n0 = Show configuration")
            if is_paired:
                print("1 = Set source call")
                print("2 = Set comment")
                print("3 = Set symbol")
                print("a = Advanced configuration")
            print("q = Disconnect and quit.")

            inp = input("Type the entry number: ")

            if inp == 'q':
                break

            if inp == 'a':
                await advanced_config(client)
                continue

            try:
                idx = int(inp)
            except ValueError:
                print("Please enter a number!")
                continue

            if idx == 0:
                print("Current configuration:")

                source_call = await client.read_gatt_char(UUID_CHAR_SOURCE_CALL)
                print(f"Source call: »{source_call.decode('utf-8')}«")

                comment = await client.read_gatt_char(UUID_CHAR_APRS_COMMENT)
                print(f"Comment:     »{comment.decode('utf-8')}«")

                symbol = await client.read_gatt_char(UUID_CHAR_APRS_SYMBOL)
                print(f"APRS symbol: »{symbol.decode('utf-8')}«\n")
            elif idx == 1:
                sourcecall = input("Type the new source call: ").strip()
                await client.write_gatt_char(UUID_CHAR_SOURCE_CALL, sourcecall.encode('utf-8'))
            elif idx == 2:
                comment = input("Type the new comment: ").strip()
                await client.write_gatt_char(UUID_CHAR_APRS_COMMENT, comment.encode('utf-8'))
            elif idx == 3:
                symbol = input("Type the new symbol code (table + icon; example: »/b« for a bicycle): ").strip()
                if len(symbol) != 2:
                    print(f"Error: exactly 2 characters are expected, you gave {len(symbol)}!")
                    continue

                await client.write_gatt_char(UUID_CHAR_APRS_SYMBOL, symbol.encode('utf-8'))
            else:
                print("Command not understood.")

    except Exception as e:
        print(e)
    finally:
        await client.disconnect()
        print("Disconnected.")

asyncio.run(main())
