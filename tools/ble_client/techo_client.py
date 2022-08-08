#!/usr/bin/env python3

import asyncio
from bleak import BleakScanner, BleakClient

UUID_CHAR_SOURCE_CALL = '00000101-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_APRS_COMMENT = '00000102-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_APRS_SYMBOL = '00000103-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_RX_MESSAGE = '00000104-b493-bb5d-2a6a-4682945c9e00'

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
            print("\n0 = Show configuration")
            if is_paired:
                print("1 = Set source call")
                print("2 = Set comment")
                print("3 = Set symbol")
            print("q = Disconnect and quit.")

            inp = input("Type the entry number: ")

            if inp == 'q':
                break

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
