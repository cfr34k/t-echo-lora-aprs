/**@file
 * @brief Driver for the SX1262 LoRa module in the T-Echo.
 * @details
 *
 * Target modulation:
 * - RF frequency: 434.400 MHz
 * - Bandwidth: 20.83 kHz (0x09)
 * - 
 */

#include <nrfx_spim.h>
#include <nrf_log.h>
#include <app_timer.h>

#include "pinout.h"
#include "periph_pwr.h"

#include "math.h"

#include "lora.h"

/*** OpCodes ***/
#define SX1262_OPCODE_SET_SLEEP              0x84
#define SX1262_OPCODE_SET_STANDBY            0x80
#define SX1262_OPCODE_SET_FREQ_SYNTH         0xC1
#define SX1262_OPCODE_SET_TX                 0x83
#define SX1262_OPCODE_SET_RX                 0x82
#define SX1262_OPCODE_STOP_TIMER_ON_PREAMBLE 0x9F
#define SX1262_OPCODE_SET_CAD                0xC5
#define SX1262_OPCODE_SET_TX_CW              0xD1
#define SX1262_OPCODE_SET_TX_INFINITE_PRE    0xD2
#define SX1262_OPCODE_SET_REGULATOR_MODE     0x96
#define SX1262_OPCODE_CALIBRATE              0x89
#define SX1262_OPCODE_CALIBRATE_IMAGE        0x98
#define SX1262_OPCODE_SET_PA_CONFIG          0x95
#define SX1262_OPCODE_SET_RX_TX_FALLBACK     0x93

#define SX1262_OPCODE_WRITE_REGISTER         0x0D
#define SX1262_OPCODE_READ_REGISTER          0x1D
#define SX1262_OPCODE_WRITE_BUFFER           0x0E
#define SX1262_OPCODE_READ_BUFFER            0x1E

#define SX1262_OPCODE_SET_DIO_IRQ_PARAMS     0x08
#define SX1262_OPCODE_GET_IRQ_STATUS         0x12
#define SX1262_OPCODE_CLEAR_IRQ_STATUS       0x02
#define SX1262_OPCODE_SET_DIO2_AS_RF_SW_CTRL 0x9D
#define SX1262_OPCODE_SET_DIO3_AS_TCXO_CTRL  0x97

#define SX1262_OPCODE_SET_RF_FREQUENCY       0x86
#define SX1262_OPCODE_SET_PACKET_TYPE        0x8A
#define SX1262_OPCODE_GET_PACKET_TYPE        0x11
#define SX1262_OPCODE_SET_TX_PARAMS          0x8E
#define SX1262_OPCODE_SET_MODULATION_PARAMS  0x8B
#define SX1262_OPCODE_SET_PACKET_PARAMS      0x8C
#define SX1262_OPCODE_SET_CAD_PARAMS         0x88
#define SX1262_OPCODE_SET_BUFFER_BASE_ADDRS  0x8F
#define SX1262_OPCODE_SET_SYMB_NUM_TIMEOUT   0xA0

#define SX1262_OPCODE_GET_STATUS             0xC0
#define SX1262_OPCODE_GET_RX_BUF_STATUS      0x13
#define SX1262_OPCODE_GET_PACKET_STATUS      0x14
#define SX1262_OPCODE_GET_RSSI_INST          0x15
#define SX1262_OPCODE_GET_STATS              0x10
#define SX1262_OPCODE_RESET_STATS            0x00
#define SX1262_OPCODE_GET_DEVICE_ERRORS      0x17
#define SX1262_OPCODE_CLEAR_DEVICE_ERRORS    0x07

/*** Modulation parameters ***/

// LoRa spreading factors
#define SX1262_LORA_SF_BYTE_IDX   1

#define SX1262_LORA_SF_5       0x05
#define SX1262_LORA_SF_6       0x06
#define SX1262_LORA_SF_7       0x07
#define SX1262_LORA_SF_8       0x08
#define SX1262_LORA_SF_9       0x09
#define SX1262_LORA_SF_10      0x0A
#define SX1262_LORA_SF_11      0x0B

// LoRa bandwidths
#define SX1262_LORA_BW_BYTE_IDX   2

#define SX1262_LORA_BW_7       0x00
#define SX1262_LORA_BW_10      0x08
#define SX1262_LORA_BW_15      0x01
#define SX1262_LORA_BW_20      0x09
#define SX1262_LORA_BW_31      0x02
#define SX1262_LORA_BW_41      0x0A
#define SX1262_LORA_BW_62      0x03
#define SX1262_LORA_BW_125     0x04
#define SX1262_LORA_BW_250     0x05
#define SX1262_LORA_BW_500     0x06

// LoRa coding rates
#define SX1262_LORA_CR_BYTE_IDX   3

#define SX1262_LORA_CR_4_5     0x01
#define SX1262_LORA_CR_4_6     0x02
#define SX1262_LORA_CR_4_7     0x03
#define SX1262_LORA_CR_4_8     0x04

// LoRa low data rate optimization
#define SX1262_LORA_LDRO_BYTE_IDX 4

#define SX1262_LORA_LDRO_OFF   0x00
#define SX1262_LORA_LDRO_ON    0x01

/*** Packet parameters ***/

// preamble length
#define SX1262_LORA_PREAMBLE_LENGTH_MSB_IDX   1
#define SX1262_LORA_PREAMBLE_LENGTH_LSB_IDX   2

// header type
#define SX1262_LORA_HEADER_TYPE_BYTE_IDX   3

#define SX1262_LORA_HEADER_TYPE_EXPLICIT 0x00
#define SX1262_LORA_HEADER_TYPE_IMPLICIT 0x01

// payload size
#define SX1262_LORA_PAYLOAD_SIZE_BYTE_IDX   4

// CRC type
#define SX1262_LORA_CRC_TYPE_BYTE_IDX   5

#define SX1262_LORA_CRC_TYPE_OFF     0x00
#define SX1262_LORA_CRC_TYPE_ON      0x01

// Invert IQ
#define SX1262_LORA_INVERT_IQ_BYTE_IDX   6

#define SX1262_LORA_INVERT_IQ_OFF     0x00
#define SX1262_LORA_INVERT_IQ_ON      0x01

/*** CAD (channel activity detection) parameters ***/

// number of symbols
#define SX1262_CAD_SYMBOL_NUM_BYTE_IDX   1

#define SX1262_CAD_ON_1_SYMB     0x00
#define SX1262_CAD_ON_2_SYMB     0x01
#define SX1262_CAD_ON_4_SYMB     0x02
#define SX1262_CAD_ON_8_SYMB     0x03
#define SX1262_CAD_ON_16_SYMB    0x04

// detector peak and minimum value
#define SX1262_CAD_DET_PEAK_BYTE_IDX    2
#define SX1262_CAD_DET_MIN_BYTE_IDX     3

// exit mode
#define SX1262_CAD_EXIT_MODE_BYTE_IDX   4

#define SX1262_CAD_EXIT_CAD_ONLY 0x00
#define SX1262_CAD_EXIT_RX       0x01

// CAD timeout
#define SX1262_CAD_TIMEOUT_MSB_BYTE_IDX   5
#define SX1262_CAD_TIMEOUT_LSB_BYTE_IDX   7


#define SX1262_PACKET_TYPE_GFSK       0x00
#define SX1262_PACKET_TYPE_LORA       0x01

#define LORA_MAX_COMMAND_LEN 9

typedef enum
{
	// for both TX and RX
	LORA_STATE_IDLE,
	LORA_STATE_WAIT_BUSY,
	LORA_STATE_RESET,
	LORA_STATE_SET_STDBY_RC,
	LORA_STATE_SET_PACKET_TYPE,
	LORA_STATE_SET_RF_FREQUENCY,
	LORA_STATE_CALIBRATE_IMAGE,
	LORA_STATE_SET_BUFFER_BASE_ADDRS,
	LORA_STATE_SET_DIO2_AS_RF_SW_CTRL,
	LORA_STATE_SET_DIO3_AS_TCXO_CTRL,
	LORA_STATE_SET_MODULATION_PARAMS,
	LORA_STATE_SET_PACKET_PARAMS,

	LORA_STATE_GET_DEVICE_ERRORS,
	LORA_STATE_CLEAR_DEVICE_ERRORS,

	// for TX only
	LORA_STATE_SET_PA_CONFIG,
	LORA_STATE_SET_TX_PARAMS,
	LORA_STATE_WRITE_BUFFER,
	LORA_STATE_SETUP_TXDONE_IRQ,
	LORA_STATE_START_TX,
	LORA_STATE_WAIT_TX_DONE,
	LORA_STATE_CLEAR_TXDONE_IRQ,

	// for RX only: TODO
} lora_state_t;

static const char *LORA_STATE_NAMES[] = {
	"IDLE",
	"WAIT_BUSY",
	"RESET",
	"SET_STDBY_RC",
	"SET_PACKET_TYPE",
	"SET_RF_FREQUENCY",
	"CALIBRATE_IMAGE",
	"SET_BUFFER_BASE_ADDRS",
	"SET_DIO2_AS_RF_SW_CTRL",
	"SET_DIO3_AS_TCXO_CTRL",
	"SET_MODULATION_PARAMS",
	"SET_PACKET_PARAMS",

	"GET_DEVICE_ERRORS",
	"CLEAR_DEVICE_ERRORS",

	// for TX only
	"SET_PA_CONFIG",
	"SET_TX_PARAMS",
	"WRITE_BUFFER",
	"SETUP_TXDONE_IRQ",
	"START_TX",
	"WAIT_TX_DONE",
	"CLEAR_TXDONE_IRQ",
};

typedef struct
{
	uint8_t rfu;
	uint8_t status;
} sx1262_status_t;

static nrfx_spim_t m_spim = NRFX_SPIM_INSTANCE(2);

APP_TIMER_DEF(m_sequence_timer);

#define TX_DONE_POLL_INTERVAL_MS 100

#define BUSY_CHECK_TICKS      APP_TIMER_TICKS(1)  // time between polls of the BUSY signal
#define RESET_TICKS           APP_TIMER_TICKS(250)  // time that a reset is applied
#define TX_DONE_CHECK_TICKS   APP_TIMER_TICKS(TX_DONE_POLL_INTERVAL_MS)  // time between polls of the DIO1 signal

static bool m_shutdown_needed = false;

static lora_state_t  m_state, m_next_state;

static uint32_t m_busy_check_counter = 0;

static sx1262_status_t m_status;

static uint8_t  m_buffer_write_command[2 + 256];
static uint8_t *m_buffer = m_buffer_write_command + 2;

#define RX_BUF_SIZE 256
static uint8_t  m_buffer_rx[RX_BUF_SIZE];

static uint8_t  m_payload_length;

static uint32_t m_tx_timeout = 600;


static float calc_toa(
		uint8_t sf,
		uint8_t cr,
		float bw_khz,
		uint8_t n_symb_pre,
		uint8_t n_bytes_payload,
		bool explicit_header,
		bool use_crc)
{
	uint8_t n_symb_header = explicit_header ? 20 : 0;
	uint8_t n_bit_crc     = use_crc ? 16 : 0;

	float arg = 8*n_bytes_payload + n_bit_crc - 4*sf + n_symb_header;

	if(arg < 0) {
		arg = 0;
	}

	float n_symb = n_symb_pre + 4.25 + 8
		+ (int)(1.0f + arg / (4*sf)) * (cr+4);

	return pow(2, sf) / bw_khz * n_symb;
}


static ret_code_t send_command(const uint8_t *command, uint16_t length, sx1262_status_t *status)
{
	nrfx_spim_xfer_desc_t xfer_desc;

	if(status && (length >= sizeof(sx1262_status_t))) {
		xfer_desc = (nrfx_spim_xfer_desc_t)
			NRFX_SPIM_XFER_TRX(command, length, (uint8_t*)status, sizeof(sx1262_status_t));
	} else {
		xfer_desc = (nrfx_spim_xfer_desc_t)NRFX_SPIM_XFER_TX(command, length);
	}

	NRF_LOG_INFO("lora: sending command (cmd: 0x%02x, length: %d).", command[0], xfer_desc.tx_length);

	nrf_gpio_pin_clear(PIN_LORA_CS);

	return nrfx_spim_xfer(&m_spim, &xfer_desc, 0);
}

static ret_code_t read_data_from_module(const uint8_t *command, uint16_t tx_length, uint8_t *rx_data, uint16_t rx_length)
{
	nrfx_spim_xfer_desc_t xfer_desc;

	xfer_desc = (nrfx_spim_xfer_desc_t)
		NRFX_SPIM_XFER_TRX(command, tx_length, rx_data, rx_length);

	NRF_LOG_INFO("lora: requesting data (cmd: 0x%02x, tx_len: %d, rx_len: %d).", command[0], xfer_desc.tx_length, xfer_desc.rx_length);

	nrf_gpio_pin_clear(PIN_LORA_CS);

	return nrfx_spim_xfer(&m_spim, &xfer_desc, 0);
}

/**@brief Run actions that should be executed when a state is about to be left.
 */
static ret_code_t handle_state_exit(void)
{
	NRF_LOG_INFO("lora: leaving state %s: status=0x%02x", LORA_STATE_NAMES[m_state], m_status.status);

	switch(m_state)
	{
		case LORA_STATE_GET_DEVICE_ERRORS:
			NRF_LOG_INFO("lora: status: 0x%02x, device errors: 0x%04x",
					m_buffer_rx[1], (m_buffer_rx[2] << 8L) | m_buffer_rx[3]);
			break;

		default:
			break;
	}

	return NRF_SUCCESS;
}

/**@brief Run actions that should be executed when a new state was just entered.
 */
static ret_code_t handle_state_entry(void)
{
	uint8_t command[LORA_MAX_COMMAND_LEN];

	NRF_LOG_INFO("lora: entering state %s", LORA_STATE_NAMES[m_state]);

	switch(m_state)
	{
		case LORA_STATE_IDLE:
			// as we enter the idle state here, we shut down all used
			// peripherals on the next main loop iteration.
			m_shutdown_needed = true;
			break;

		case LORA_STATE_WAIT_BUSY:
			VERIFY_SUCCESS(app_timer_start(m_sequence_timer, BUSY_CHECK_TICKS, NULL));
			break;

		case LORA_STATE_RESET:
			nrf_gpio_pin_clear(PIN_LORA_RST);
			nrf_gpio_cfg_output(PIN_LORA_RST);

			VERIFY_SUCCESS(app_timer_start(m_sequence_timer, RESET_TICKS, NULL));
			break;

		case LORA_STATE_SET_STDBY_RC:
			command[0] = SX1262_OPCODE_SET_STANDBY;
			command[1] = 0x00; // STDBY_RC

			APP_ERROR_CHECK(send_command(command, 2, &m_status));
			break;

		case LORA_STATE_SET_PACKET_TYPE:
			command[0] = SX1262_OPCODE_SET_PACKET_TYPE;
			command[1] = SX1262_PACKET_TYPE_LORA;

			APP_ERROR_CHECK(send_command(command, 2, &m_status));
			break;

		case LORA_STATE_SET_RF_FREQUENCY:
			// frequency = value * f_xtal / 2^25
			// => value = 434.4 MHz * 2^25 / (32 MHz) = 455501414.4 = 0x1b266666
			command[0] = SX1262_OPCODE_SET_RF_FREQUENCY;
			command[1] = 0x1B;
			command[2] = 0x26;
			command[3] = 0x66;
			command[4] = 0x66;

			APP_ERROR_CHECK(send_command(command, 5, &m_status));
			break;

		case LORA_STATE_CALIBRATE_IMAGE:
			command[0] = SX1262_OPCODE_CALIBRATE_IMAGE;
			command[1] = 0x6B; // 430 ..
			command[2] = 0x6F; // 440 MHz

			APP_ERROR_CHECK(send_command(command, 3, &m_status));
			break;

		case LORA_STATE_SET_BUFFER_BASE_ADDRS:
			// Set buffer base addresses to 0
			command[0] = SX1262_OPCODE_SET_BUFFER_BASE_ADDRS;
			command[1] = 0x00;
			command[2] = 0x00;

			APP_ERROR_CHECK(send_command(command, 3, &m_status));
			break;

		case LORA_STATE_SET_DIO2_AS_RF_SW_CTRL:
			command[0] = SX1262_OPCODE_SET_DIO2_AS_RF_SW_CTRL;
			command[1] = 0x01; // enable RF switch control

			APP_ERROR_CHECK(send_command(command, 2, &m_status));
			break;

		case LORA_STATE_SET_DIO3_AS_TCXO_CTRL:
			command[0] = SX1262_OPCODE_SET_DIO3_AS_TCXO_CTRL;
			command[1] = 0x07; // supply 3.3V to the TCXO
			command[2] = 0x00; // bytes 2..4: timeout
			command[3] = 0x02; // 10 ms in steps of 15.625 μs
			command[4] = 0x80;

			APP_ERROR_CHECK(send_command(command, 5, &m_status));
			break;

		case LORA_STATE_SET_MODULATION_PARAMS:
			command[0] = SX1262_OPCODE_SET_MODULATION_PARAMS;
			command[1] = SX1262_LORA_SF_9;
			command[2] = SX1262_LORA_BW_20;
			command[3] = SX1262_LORA_CR_4_5;
			command[4] = SX1262_LORA_LDRO_OFF;

			APP_ERROR_CHECK(send_command(command, 5, &m_status));
			break;

		case LORA_STATE_SET_PACKET_PARAMS:
			command[0] = SX1262_OPCODE_SET_PACKET_PARAMS;
			command[1] = 0x00; // 16 preamble symbols: MSB
			command[2] = 0x10; // 16 preamble symbols: LSB
			command[3] = SX1262_LORA_HEADER_TYPE_EXPLICIT;
			command[4] = m_payload_length;
			command[5] = SX1262_LORA_CRC_TYPE_ON;
			command[6] = SX1262_LORA_INVERT_IQ_OFF;

			APP_ERROR_CHECK(send_command(command, 7, &m_status));

			/* NOTE! This is the last common state for TX and RX. The path used
			 * is determined in cb_spim(). */
			break;

		case LORA_STATE_GET_DEVICE_ERRORS:
			command[0] = SX1262_OPCODE_GET_DEVICE_ERRORS;
			command[1] = 0x00;
			command[2] = 0x00;
			command[3] = 0x00;

			APP_ERROR_CHECK(read_data_from_module(command, 4, m_buffer_rx, 4));
			break;

		case LORA_STATE_CLEAR_DEVICE_ERRORS:
			command[0] = SX1262_OPCODE_CLEAR_DEVICE_ERRORS;
			command[1] = 0x00;
			command[2] = 0x00;

			APP_ERROR_CHECK(send_command(command, 3, &m_status));
			break;

		/* The following states are only used in TX. */

		case LORA_STATE_SET_PA_CONFIG:
			// optimal PA settings for +14 dBm, see datasheet page 77
			command[0] = SX1262_OPCODE_SET_PA_CONFIG;
			command[1] = 0x02;
			command[2] = 0x02;
			command[3] = 0x00;
			command[4] = 0x01;

			APP_ERROR_CHECK(send_command(command, 5, &m_status));
			break;

		case LORA_STATE_SET_TX_PARAMS:
			// TX parameters: "22" dBm, 200 μs ramp time
			command[0] = SX1262_OPCODE_SET_TX_PARAMS;
			command[1] = 0x16;
			command[2] = 0x04;

			APP_ERROR_CHECK(send_command(command, 3, &m_status));
			break;

		case LORA_STATE_WRITE_BUFFER:
			APP_ERROR_CHECK(send_command(m_buffer_write_command, 2 + m_payload_length, &m_status));
			break;

		case LORA_STATE_SETUP_TXDONE_IRQ:
			command[0] = SX1262_OPCODE_SET_DIO_IRQ_PARAMS;
			command[1] = 0x02; // IRQ Mask: MSB
			command[2] = 0x01; // IRQ Mask: LSB => TxDone or Timeout
			command[3] = 0x02; // DIO1 Mask: MSB
			command[4] = 0x01; // DIO1 Mask: LSB => TxDone or Timeout
			command[5] = 0x00; // DIO2 Mask: MSB
			command[6] = 0x00; // DIO2 Mask: LSB
			command[7] = 0x00; // DIO3 Mask: MSB
			command[8] = 0x00; // DIO3 Mask: LSB

			APP_ERROR_CHECK(send_command(command, 9, &m_status));
			break;

		case LORA_STATE_START_TX:
			{
				float toa = calc_toa(
						9,      // SF
						1,      // CR=4/5
						20.88f, // BW
						16,     // preamble symbols
						m_payload_length,
						true,   // explicit header
						true);  // use CRC

				m_tx_timeout = 1.20f * toa * 1000.0f / TX_DONE_POLL_INTERVAL_MS;

				NRF_LOG_INFO("lora: expected time on air: %d ms", (int)(toa));
			}

			command[0] = SX1262_OPCODE_SET_TX;
			command[1] = 0x04; // Bytes 1..3: timeout
			command[2] = 0xE2; // 1 LSB = 15.625 μs
			command[3] = 0x00; // => 5 seconds timeout

			APP_ERROR_CHECK(send_command(command, 4, &m_status));
			break;

		case LORA_STATE_WAIT_TX_DONE:
			VERIFY_SUCCESS(app_timer_start(m_sequence_timer, TX_DONE_CHECK_TICKS, NULL));
			break;

		case LORA_STATE_CLEAR_TXDONE_IRQ:
			command[0] = SX1262_OPCODE_CLEAR_IRQ_STATUS;
			command[1] = 0x01; // Clear timeout
			command[2] = 0x01; // and TxDone IRQs

			APP_ERROR_CHECK(send_command(command, 3, &m_status));
			break;
	}

	return NRF_SUCCESS;
}

/**@brief Move to a new state in the FSM, calling state exit and entry functions on the way.
 */
static void transit_to_state(lora_state_t new_state)
{
	APP_ERROR_CHECK(handle_state_exit());
	m_state = new_state;
	APP_ERROR_CHECK(handle_state_entry());
}

static void cb_spim(nrfx_spim_evt_t const *p_event, void *p_context)
{
	nrf_gpio_pin_set(PIN_LORA_CS);

	switch(m_state)
	{
		case LORA_STATE_SET_STDBY_RC:
			m_next_state = LORA_STATE_SET_DIO3_AS_TCXO_CTRL;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SET_DIO3_AS_TCXO_CTRL:
			m_next_state = LORA_STATE_CALIBRATE_IMAGE;
			transit_to_state(LORA_STATE_CLEAR_DEVICE_ERRORS);
			break;

		case LORA_STATE_CALIBRATE_IMAGE:
			m_next_state = LORA_STATE_SET_PACKET_TYPE;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SET_PACKET_TYPE:
			m_next_state = LORA_STATE_SET_MODULATION_PARAMS;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SET_MODULATION_PARAMS:
			m_next_state = LORA_STATE_SET_PACKET_PARAMS;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SET_PACKET_PARAMS:
			m_next_state = LORA_STATE_SET_RF_FREQUENCY;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SET_RF_FREQUENCY:
			m_next_state = LORA_STATE_SET_BUFFER_BASE_ADDRS;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SET_BUFFER_BASE_ADDRS:
			m_next_state = LORA_STATE_SET_DIO2_AS_RF_SW_CTRL;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SET_DIO2_AS_RF_SW_CTRL:
			// TODO: switch between TX and RX.
			// For now, TX only
			m_next_state = LORA_STATE_SET_PA_CONFIG;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

			/* Generic, reusable states */

		case LORA_STATE_GET_DEVICE_ERRORS:
			transit_to_state(m_next_state);
			break;

		case LORA_STATE_CLEAR_DEVICE_ERRORS:
			transit_to_state(m_next_state);
			break;

			/* The following states are only used in TX. */

		case LORA_STATE_SET_PA_CONFIG:
			m_next_state = LORA_STATE_SET_TX_PARAMS;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SET_TX_PARAMS:
			m_next_state = LORA_STATE_WRITE_BUFFER;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_WRITE_BUFFER:
			m_next_state = LORA_STATE_SETUP_TXDONE_IRQ;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_SETUP_TXDONE_IRQ:
			m_next_state = LORA_STATE_START_TX;
			transit_to_state(LORA_STATE_GET_DEVICE_ERRORS);
			break;

		case LORA_STATE_START_TX:
			m_next_state = LORA_STATE_WAIT_TX_DONE;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_CLEAR_TXDONE_IRQ:
			m_next_state = LORA_STATE_IDLE;
			transit_to_state(LORA_STATE_GET_DEVICE_ERRORS);
			break;

		default:
			NRF_LOG_ERROR("lora: cb_spim() called in unexpected state: %s", LORA_STATE_NAMES[m_state]);
			break;
	}
}

static void cb_sequence_timer(void *p_context)
{
	switch(m_state)
	{
		case LORA_STATE_RESET:
			NRF_LOG_INFO("lora: reset complete.");

			nrf_gpio_cfg_input(PIN_LORA_RST, NRF_GPIO_PIN_PULLUP);

			m_next_state = LORA_STATE_SET_STDBY_RC;
			transit_to_state(LORA_STATE_WAIT_BUSY);
			break;

		case LORA_STATE_WAIT_BUSY:
			if(nrf_gpio_pin_read(PIN_LORA_BUSY)) {
				// still busy, restart the timer
				m_busy_check_counter++;
				APP_ERROR_CHECK(app_timer_start(m_sequence_timer, BUSY_CHECK_TICKS, NULL));
			} else {
				// not busy anymore => send the next command
				NRF_LOG_INFO("lora: busy flag released after %d polls.", m_busy_check_counter);
				m_busy_check_counter = 0;
				transit_to_state(m_next_state);
			}
			break;

		case LORA_STATE_WAIT_TX_DONE:
			if(!nrf_gpio_pin_read(PIN_LORA_DIO1) && (m_busy_check_counter < m_tx_timeout)) {
				// still not done, restart the timer
				m_busy_check_counter++;
				APP_ERROR_CHECK(app_timer_start(m_sequence_timer, TX_DONE_CHECK_TICKS, NULL));
			} else {
				if(m_busy_check_counter == m_tx_timeout) {
					NRF_LOG_ERROR("lora: tx_done timed out after %d polls.", m_busy_check_counter);
				} else {
					// done!
					NRF_LOG_INFO("lora: tx_done signalled after %d polls.", m_busy_check_counter);
				}

				m_busy_check_counter = 0;
				transit_to_state(LORA_STATE_CLEAR_TXDONE_IRQ);
			}
			break;

		default:
			NRF_LOG_ERROR("lora: cb_sequence_timer() called in unexpected state: %s", LORA_STATE_NAMES[m_state]);
			break;
	}
}


void lora_config_gpios(bool power_supplied)
{
	nrf_gpio_cfg_default(PIN_LORA_MISO);
	nrf_gpio_cfg_default(PIN_LORA_MOSI);
	nrf_gpio_cfg_default(PIN_LORA_SCK);
	nrf_gpio_cfg_default(PIN_LORA_DIO3);

	if(power_supplied) {
		// while power is on, it is better to keep the pullups on Chip Select and
		// the Reset pin to prevent noise causing spurious commands to be executed.
		nrf_gpio_cfg_input(PIN_LORA_CS, NRF_GPIO_PIN_PULLUP);
		nrf_gpio_cfg_input(PIN_LORA_RST, NRF_GPIO_PIN_PULLUP);
	} else {
		// reset all SPI I/Os to the default state (input). Otherwise a lot of
		// current (~10 mA) flows through the protection diodes of the module once
		// the supply voltage is switched off.
		nrf_gpio_cfg_default(PIN_LORA_CS);
		nrf_gpio_cfg_default(PIN_LORA_RST);
	}
}


ret_code_t lora_init(void)
{
	// initialize the GPIOs.
	nrf_gpio_cfg_default(PIN_LORA_RST);
	nrf_gpio_cfg_input(PIN_LORA_BUSY, NRF_GPIO_PIN_NOPULL);

	// SPI will be initialized on demand.
	nrf_gpio_cfg_default(PIN_LORA_CS);

	// prepare the buffer transfer command
	m_buffer_write_command[0] = SX1262_OPCODE_WRITE_BUFFER;
	m_buffer_write_command[1] = 0x00; // start offset

	NRF_LOG_INFO("lora: init.");

	m_state = LORA_STATE_IDLE;

	return app_timer_create(&m_sequence_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_sequence_timer);
}

ret_code_t lora_send_packet(uint8_t *data, uint8_t length)
{
	if(m_state != LORA_STATE_IDLE) {
		return NRF_ERROR_BUSY;
	}

	periph_pwr_start_activity(PERIPH_PWR_FLAG_LORA);

	nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
	spi_config.frequency      = NRF_SPIM_FREQ_2M;
	spi_config.ss_pin         = NRFX_SPIM_PIN_NOT_USED; // CS is controlled manually
	//spi_config.ss_pin         = PIN_LORA_CS;
	spi_config.miso_pin       = PIN_LORA_MISO;
	spi_config.mosi_pin       = PIN_LORA_MOSI;
	spi_config.sck_pin        = PIN_LORA_SCK;

	VERIFY_SUCCESS(nrfx_spim_init(&m_spim, &spi_config, cb_spim, NULL));

	nrf_gpio_pin_set(PIN_LORA_CS);
	nrf_gpio_cfg_output(PIN_LORA_CS);

#if 0
	// according to the Devzone, SPI at 8 MHz requires high drive outputs
	nrf_gpio_cfg(PIN_LORA_CS,   NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(PIN_LORA_MOSI, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(PIN_LORA_SCK,  NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(PIN_LORA_DC,   NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
#endif

	// copy the data to send
	m_payload_length = length;
	memcpy(m_buffer, data, length);

	// module has been powered on; reset it for proper startup
	NRF_LOG_INFO("lora: Resetting module.");
	transit_to_state(LORA_STATE_RESET);

	// the rest will happen asynchronously. See cb_sequence_timer() and cb_spim().
	m_shutdown_needed = false;

	return NRF_SUCCESS;
}


bool lora_is_busy(void)
{
	return (m_state != LORA_STATE_IDLE);
}


void lora_loop(void)
{
	if(m_shutdown_needed) {
		nrfx_spim_uninit(&m_spim); // to save power

		lora_config_gpios(true); // safe powered state

		periph_pwr_stop_activity(PERIPH_PWR_FLAG_LORA);

		m_shutdown_needed = false;
	}
}
