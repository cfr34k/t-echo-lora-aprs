# Interface to the low-level settings characteristics on the T-Echo.

import struct

import menu

SETTINGS_IDS = {
        'SOURCE_CALL':       0x0001,
        'SYMBOL_CODE':       0x0002,
        'COMMENT':           0x0003,
        'LORA_POWER':        0x0004,
        'APRS_FLAGS':        0x0005,
        'LAST_BLE_SYMBOL':   0x0006,
        'LORA_RF_FREQUENCY': 0x0007,
        'LORA_MOD_PARAMS':   0x0008,
    }

APRS_FLAGS = {
        'COMPRESS_LOCATION': 1 << 0,
        'ADD_DAO':           1 << 1,
        'ADD_FRAME_COUNTER': 1 << 2,
        'ADD_ALTITUDE':      1 << 3,
        'ADD_VBAT':          1 << 4,
        'ADD_WEATHER':       1 << 5,
        'STARTUP_RX_ON':     1 << 6,
        'STARTUP_TX_ON':     1 << 7,
    }

MOD_PARAMS_SF = {
        0x05: 'SF 5',
        0x06: 'SF 6',
        0x07: 'SF 7',
        0x08: 'SF 8',
        0x09: 'SF 9',
        0x0A: 'SF 10',
        0x0B: 'SF 11',
        0x0C: 'SF 12',
    }

MOD_PARAMS_CR = {
        0x01: 'CR 4/5',
        0x02: 'CR 4/6',
        0x03: 'CR 4/7',
        0x04: 'CR 4/8',
    }

MOD_PARAMS_BW = {
        0x00: '7.81 kHz',
        0x08: '10.42 kHz',
        0x01: '15.63 kHz',
        0x09: '20.83 kHz',
        0x02: '31.25 kHz',
        0x0A: '41.67 kHz',
        0x03: '62.50 kHz',
        0x04: '125 kHz',
        0x05: '250 kHz',
        0x06: '500 kHz',
    }

MOD_PARAMS_LDRO = {
        0x00: 'OFF',
        0x01: 'ON',
    }

LORA_POWERS_DBM = [22, 20, 17, 14, 10, 0, -9]

UUID_CHAR_SETTING_WRITE_SELECT = '00000110-b493-bb5d-2a6a-4682945c9e00'
UUID_CHAR_SETTING_READ = '00000111-b493-bb5d-2a6a-4682945c9e00'

class Setting:
    def __init__(self, name, data):
        self.name = name
        self.data = data

        self.modified = False
        self.default = not data

    def format(self):
        if self.data is None:
            return '[None/Default]'

        if self.name in ['SOURCE_CALL', 'COMMENT', 'SYMBOL_CODE', 'LAST_BLE_SYMBOL']:
            null_idx = self.data.index(0)
            return self.data[:null_idx].decode('utf-8')
        elif self.name == 'LORA_POWER':
            index = self.data[0]
            return f"{LORA_POWERS_DBM[index]} dBm"
        elif self.name == 'APRS_FLAGS':
            decoded, = struct.unpack('<I', self.data)
            return f"0x{decoded:08x}"
        elif self.name == 'LORA_RF_FREQUENCY':
            freq, = struct.unpack('<I', self.data)
            return f"{freq} Hz"
        elif self.name == 'LORA_MOD_PARAMS':
            sf, bw, cr, ldro = struct.unpack('4B', self.data)
            sf_s = MOD_PARAMS_SF[sf]
            bw_s = MOD_PARAMS_BW[bw]
            cr_s = MOD_PARAMS_CR[cr]
            ldro_s = MOD_PARAMS_LDRO[ldro]
            return f"{sf_s}, {bw_s}, {cr_s}, {ldro_s}"
        else:
            return f"{self.data}"

    def _edit_strval(self):
        print(f'Enter new value for {self.name}.')
        print("Leave empty to abort.")
        print("Enter a single dot (.) to write an empty string.")
        inp = input('> ')

        if not inp:
            return False # canceled

        if inp == '.':
            self.data = b'\x00'
        else:
            self.data = inp.encode('utf-8') + b'\x00'

        return True

    def _edit_symbol(self):
        print(f'Enter a new symbol code for {self.name}.')
        print("Exactly two ASCII characters are required. Leave empty to abort.")

        while True:
            inp = input('> ')

            if not inp:
                return False # canceled

            if len(inp) == 2:
                self.data = inp.encode('utf-8') + b'\x00'
                return True
            else:
                print(f"Exactly 2 characters required. You entered {len(inp)} characters. Try again.")

    def _edit_power(self):
        power_strings = [f"{p:3d} dBm" for p in LORA_POWERS_DBM]
        options = dict(zip(range(len(LORA_POWERS_DBM)), LORA_POWERS_DBM))

        selected = menu.choose_option("Select the new power setting:", options)

        if not selected:
            return False
        else:
            self.data = bytes([int(selected)])
            return True

    def _edit_flags(self):
        initial_flags = flags = struct.unpack('<I', self.data)[0]

        while True:
            print("Flag state:\n")

            for name, flag in APRS_FLAGS.items():
                if flags & flag != 0:
                    state = 'set'
                else:
                    state = 'unset'

                print(f"{name+':':18s} {state:>5s}")

            print()

            flag_names = list(APRS_FLAGS.keys())
            options = dict(zip(range(len(APRS_FLAGS)), flag_names))

            selected = menu.choose_option("Select a flag to toggle:", options)

            if not selected:
                break
            else:
                # toggle the selected flag
                flag_name = flag_names[int(selected)]
                flags ^= APRS_FLAGS[flag_name]

        self.data = struct.pack("<I", flags)
        return flags != initial_flags

    def _edit_rf_frequency(self):
        print('Enter a new RF frequency in Hz (e.g. "433775000" for 433.775 MHz).')
        print('Leave empty to abort.')

        while True:
            inp = input('> ')

            if not inp:
                return False

            try:
                freq = int(inp)
            except ValueError:
                print("Could not parse your input as integer. Try again.\n")
                continue

            if freq < 150e6 or freq > 960e6:
                print("Your input is not in the supported range of the SX1262 module (150 to 960 MHz). Try again.\n")
                continue

            self.data = struct.pack("<I", freq)
            return True

    def _edit_mod_params(self):
        if not self.data:
            print("\n!!!")
            print("WARNING: modulation parameters are not set yet on the device. Assuming default values.")
            print("!!!\n")
            local_data = [0x0C, 0x04, 0x01, 0x01]
        else:
            local_data = list(self.data)

        modified = False

        while True:
            options = {
                    '0': f"Spreading Factor [{MOD_PARAMS_SF[local_data[0]]}]",
                    '1': f"Bandwidth [{MOD_PARAMS_BW[local_data[1]]}]",
                    '2': f"Coding Rate [{MOD_PARAMS_CR[local_data[2]]}]",
                    '3': f"Low Data Rate Optimization [{MOD_PARAMS_LDRO[local_data[3]]}]",
                }

            selected = menu.choose_option("Select the parameter to edit:", options)

            if not selected:
                self.data = bytes(local_data)
                return modified
            elif selected == '0':
                options = dict(zip(range(len(MOD_PARAMS_SF)), MOD_PARAMS_SF.values()))
                selected_sf = menu.choose_option("Select the new spreading factor:", options)
                if selected_sf != 'q':
                    local_data[0] = list(MOD_PARAMS_SF.keys())[int(selected_sf)]
                    modified = True
            elif selected == '1':
                options = dict(zip(range(len(MOD_PARAMS_BW)), MOD_PARAMS_BW.values()))
                selected_bw = menu.choose_option("Select the new bandwidth:", options)
                if selected_bw != 'q':
                    local_data[1] = list(MOD_PARAMS_BW.keys())[int(selected_bw)]
                    modified = True
            elif selected == '2':
                options = dict(zip(range(len(MOD_PARAMS_CR)), MOD_PARAMS_CR.values()))
                selected_cr = menu.choose_option("Select the new coding rate:", options)
                if selected_cr != 'q':
                    local_data[2] = list(MOD_PARAMS_CR.keys())[int(selected_cr)]
                    modified = True
            elif selected == '3':
                options = dict(zip(range(len(MOD_PARAMS_LDRO)), MOD_PARAMS_LDRO.values()))
                selected_ldro = menu.choose_option("Enable Low Data Rate Optimization?", options)
                if selected_ldro != 'q':
                    local_data[3] = list(MOD_PARAMS_LDRO.keys())[int(selected_ldro)]
                    modified = True

    def edit_interactive(self):
        modified = False

        if self.name in ['SOURCE_CALL', 'COMMENT']:
            modified = self._edit_strval()
        elif self.name in ['SYMBOL_CODE', 'LAST_BLE_SYMBOL']:
            modified = self._edit_symbol()
        elif self.name == 'LORA_POWER':
            modified = self._edit_power()
        elif self.name == 'APRS_FLAGS':
            modified = self._edit_flags()
        elif self.name == 'LORA_RF_FREQUENCY':
            modified = self._edit_rf_frequency()
        elif self.name == 'LORA_MOD_PARAMS':
            modified = self._edit_mod_params()

        if modified:
            self.modified = True


class AdvancedSettings:
    def __init__(self, client):
        self._ble_client = client

        self._cache = {}

    async def _read_value(self, setting_id):
        await self._ble_client.write_gatt_char(UUID_CHAR_SETTING_WRITE_SELECT, struct.pack('B', setting_id))
        raw_data = await self._ble_client.read_gatt_char(UUID_CHAR_SETTING_READ)

        if raw_data[0] & 0x80 != 0:
            print(f"Error: could not read setting #{setting_id}.")
            return None

        if raw_data[0] & 0x7F != setting_id:
            print(f"Error: device returned wrong setting ID. Requested #{setting_id}, received #{raw_data[0] & 0x7F}.")
            return None

        return raw_data[1:]

    async def _write_value(self, setting_id, data):
        await self._ble_client.write_gatt_char(UUID_CHAR_SETTING_WRITE_SELECT, struct.pack('B', setting_id) + data)
        raw_data = await self._ble_client.read_gatt_char(UUID_CHAR_SETTING_READ)

        if raw_data[0] & 0x80 != 0:
            print(f"Error: could not update setting #{setting_id}.")
            return False

        if raw_data[0] & 0x7F != setting_id:
            print(f"Error: device returned wrong setting ID. Requested #{setting_id}, received #{raw_data[0] & 0x7F}.")
            return False

        return True

    async def update_cache(self):
        print("Updating settings cache...")

        i = 1
        for name, setting_id in SETTINGS_IDS.items():
            data = await self._read_value(setting_id)

            self._cache[name] = Setting(name, data)

            print(f"\r{i * 100 / len(SETTINGS_IDS):.0f} % loaded.", end='', flush=True)
            i += 1

        print()

    def show_cache(self):
        print("Current configuration:")

        for name, setting_id in SETTINGS_IDS.items():
            setting = self.get_value(name)
            formatted = setting.format()

            flags = list("  ")

            if setting.modified:
                flags[0] = 'm'

            if setting.default:
                flags[1] = 'd'

            flags = ''.join(flags)

            print(f"{setting_id:3d} {name:20s} [{flags}] -> {formatted}")

    async def save_modified_settings(self):
        overall_success = True

        for name, setting in self._cache.items():
            if not setting.modified:
                continue

            print(f"Saving {name} ({len(setting.data)} bytes) ...")

            success = await self._write_value(SETTINGS_IDS[name], setting.data)

            if success:
                setting.modified = False
                print("Done.")
            else:
                print(f"Error: {name} could not be written with value {setting.data}.")
                overall_success = False

        return overall_success

    def get_value(self, name):
        return self._cache[name]
