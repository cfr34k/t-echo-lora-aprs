#include <nrfx_twim.h>
#include <nrf_error.h>
#include <sdk_macros.h>
#include <app_timer.h>

#define NRF_LOG_MODULE_NAME bme280
#include <nrf_log.h>
NRF_LOG_MODULE_REGISTER();

#include "bme280_comp.h"
#include "pinout.h"
#include "periph_pwr.h"

#include "bme280.h"

typedef enum {
	BME280_STATE_COMMUNICATION_ERROR,

	BME280_STATE_RESET,
	BME280_STATE_READ_CHIPID,
	BME280_STATE_READ_CAL1,
	BME280_STATE_READ_CAL2,
	BME280_STATE_NOT_PRESENT,
	BME280_STATE_INITIALIZED,
	BME280_STATE_START_MEASUREMENT,
	BME280_STATE_CHECK_COMPLETION,
	BME280_STATE_READOUT,

	BME280_NUM_STATES
} bme280_state_t;

static const char *BME280_STATE_NAMES[BME280_NUM_STATES] = {
	"COMMUNICATION_ERROR",

	"RESET",
	"READ_CHIPID",
	"READ_CAL1",
	"READ_CAL2",
	"NOT_PRESENT",
	"INITIALIZED",
	"START_MEASUREMENT",
	"CHECK_COMPLETION",
	"READOUT",
};

// calibration value cache, shared with bme280_comp.c

extern uint16_t dig_T1;
extern  int16_t dig_T2;
extern  int16_t dig_T3;

extern uint16_t dig_P1;
extern  int16_t dig_P2;
extern  int16_t dig_P3;
extern  int16_t dig_P4;
extern  int16_t dig_P5;
extern  int16_t dig_P6;
extern  int16_t dig_P7;
extern  int16_t dig_P8;
extern  int16_t dig_P9;

extern uint8_t  dig_H1;
extern  int16_t dig_H2;
extern uint8_t  dig_H3;
extern  int16_t dig_H4;
extern  int16_t dig_H5;
extern  int8_t  dig_H6;


static bme280_callback_t m_callback;

static bme280_state_t m_state;

static nrfx_twim_t m_twim = NRFX_TWIM_INSTANCE(1);

static uint8_t m_twi_tx_buf[32];
static uint8_t m_twi_rx_buf[32];

static float m_temperature;
static float m_humidity;
static float m_pressure;

APP_TIMER_DEF(m_delay_timer);

static ret_code_t start_transfer_for_current_state(void)
{
	uint8_t bytes2send = 0;
	uint8_t bytes2receive = 0;

	switch(m_state)
	{
		case BME280_STATE_RESET:
			m_twi_tx_buf[bytes2send++] = 0xE0; // reset register
			m_twi_tx_buf[bytes2send++] = 0xB6; // magic word
			break;

		case BME280_STATE_READ_CHIPID:
			m_twi_tx_buf[bytes2send++] = 0xD0; // chip ID register
			bytes2send = 1;
			bytes2receive = 1;
			break;

		case BME280_STATE_READ_CAL1:
			m_twi_tx_buf[bytes2send++] = 0x88; // first calibration data register of first block
			bytes2receive = 0xA1 + 1 - 0x88;
			break;

		case BME280_STATE_READ_CAL2:
			m_twi_tx_buf[bytes2send++] = 0xE1; // first calibration data register of first block
			bytes2receive = 0xE7 + 1 - 0xE1;
			break;

		case BME280_STATE_START_MEASUREMENT:
			m_twi_tx_buf[bytes2send++] = 0xF2; // humidity control register
			m_twi_tx_buf[bytes2send++] = 0x01; // enable, oversampling x1
			m_twi_tx_buf[bytes2send++] = 0xF4; // measurement control register
			m_twi_tx_buf[bytes2send++] = (0x01 << 5) | (0x01 << 2) | 0x01; // pressure x1, temp x1, forced mode
			break;

		case BME280_STATE_CHECK_COMPLETION:
			m_twi_tx_buf[bytes2send++] = 0xF3; // status register
			bytes2receive = 1;
			break;

		case BME280_STATE_READOUT:
			// read all the data registers in a burst, starting at press_msb
			m_twi_tx_buf[bytes2send++] = 0xF7;
			bytes2receive = 8;
			break;

		default:
			NRF_LOG_ERROR("Starting a transfer is not supported in state %s", BME280_STATE_NAMES[m_state]);
			return NRF_ERROR_INVALID_STATE;
	}

	NRF_LOG_INFO("Starting transfer for state %s: tx: %d, rx: %d", BME280_STATE_NAMES[m_state], bytes2send, bytes2receive);

	nrfx_twim_xfer_desc_t xfer;

	if(bytes2send != 0 && bytes2receive != 0) {
		// TRX
		xfer = (nrfx_twim_xfer_desc_t)NRFX_TWIM_XFER_DESC_TXRX(
				BME280_7BIT_ADDR, m_twi_tx_buf, bytes2send, m_twi_rx_buf, bytes2receive);
	} else if(bytes2send != 0) {
		// TX only
		xfer = (nrfx_twim_xfer_desc_t)NRFX_TWIM_XFER_DESC_TX(
				BME280_7BIT_ADDR, m_twi_tx_buf, bytes2send);
	} else if(bytes2send != 0) {
		// RX only
		xfer = (nrfx_twim_xfer_desc_t)NRFX_TWIM_XFER_DESC_RX(
				BME280_7BIT_ADDR, m_twi_rx_buf, bytes2receive);
	} else {
		return NRF_ERROR_NOT_SUPPORTED;
	}

	return nrfx_twim_xfer(&m_twim, &xfer, 0);
}


static ret_code_t handle_completed_transfer(const nrfx_twim_xfer_desc_t *transfer_desc)
{
	switch(m_state)
	{
		case BME280_STATE_RESET:
			// TODO: sleep a bit because of restart delay?
			m_state = BME280_STATE_READ_CHIPID;
			return start_transfer_for_current_state();

		case BME280_STATE_READ_CHIPID:
			// check whether this really is a BME280
			if(m_twi_rx_buf[0] != 0x60) {
				m_state = BME280_STATE_NOT_PRESENT;
				bme280_powersave();
				m_callback(BME280_EVT_INIT_NOT_PRESENT);
			} else {
				m_state = BME280_STATE_READ_CAL1;
				return start_transfer_for_current_state();
			}
			break;

		case BME280_STATE_READ_CAL1:
			dig_T1 = (((uint16_t)m_twi_rx_buf[ 1]) << 8) | m_twi_rx_buf[ 0]; // 0x88, 0x89
			dig_T2 =  (((int16_t)(int8_t)m_twi_rx_buf[ 3]) << 8) | m_twi_rx_buf[ 2]; // 0x8A, 0x8B
			dig_T3 =  (((int16_t)(int8_t)m_twi_rx_buf[ 5]) << 8) | m_twi_rx_buf[ 4]; // 0x8C, 0x8D

			dig_P1 = (((uint16_t)m_twi_rx_buf[ 7]) << 8) | m_twi_rx_buf[ 6]; // 0x8E, 0x8F
			dig_P2 =  (((int16_t)(int8_t)m_twi_rx_buf[ 9]) << 8) | m_twi_rx_buf[ 8]; // 0x90, 0x91
			dig_P3 =  (((int16_t)(int8_t)m_twi_rx_buf[11]) << 8) | m_twi_rx_buf[10]; // 0x92, 0x93
			dig_P4 =  (((int16_t)(int8_t)m_twi_rx_buf[13]) << 8) | m_twi_rx_buf[12]; // 0x94, 0x95
			dig_P5 =  (((int16_t)(int8_t)m_twi_rx_buf[15]) << 8) | m_twi_rx_buf[14]; // 0x96, 0x97
			dig_P6 =  (((int16_t)(int8_t)m_twi_rx_buf[17]) << 8) | m_twi_rx_buf[16]; // 0x98, 0x99
			dig_P7 =  (((int16_t)(int8_t)m_twi_rx_buf[19]) << 8) | m_twi_rx_buf[18]; // 0x9A, 0x9B
			dig_P8 =  (((int16_t)(int8_t)m_twi_rx_buf[21]) << 8) | m_twi_rx_buf[20]; // 0x9C, 0x9D
			dig_P9 =  (((int16_t)(int8_t)m_twi_rx_buf[23]) << 8) | m_twi_rx_buf[22]; // 0x9E, 0x9F

			dig_H1 = m_twi_rx_buf[25]; // 0xA1; note: m_twi_rx_buf[24] unused

			m_state = BME280_STATE_READ_CAL2;
			return start_transfer_for_current_state();

		case BME280_STATE_READ_CAL2:
			dig_H2 =  (((int16_t)(int8_t)m_twi_rx_buf[1]) << 8) | m_twi_rx_buf[0]; // 0xE1, 0xE2

			dig_H3 = m_twi_rx_buf[2]; // 0xE3

			dig_H4 =  (((int16_t)(int8_t)m_twi_rx_buf[3]) << 4) | (m_twi_rx_buf[4] & 0x0F); // 0xE4, 0xE5
			dig_H5 =  (((int16_t)(int8_t)m_twi_rx_buf[5]) << 4) | ((m_twi_rx_buf[4] & 0xF0) >> 4); // 0xE5, 0xE6

			dig_H6 = m_twi_rx_buf[6]; // 0xE7

			m_state = BME280_STATE_INITIALIZED;
			bme280_powersave();
			m_callback(BME280_EVT_INIT_DONE);
			break;

		case BME280_STATE_START_MEASUREMENT:
			m_state = BME280_STATE_CHECK_COMPLETION;
			return app_timer_start(m_delay_timer, APP_TIMER_TICKS(5), NULL);

		case BME280_STATE_CHECK_COMPLETION:
			if((m_twi_rx_buf[0] & 0x09) != 0) {
				// either "measuring" or "im_update" is active, so measurement is not complete yet.
				// => check status again
				return start_transfer_for_current_state();
			} else {
				m_state = BME280_STATE_READOUT;
				return start_transfer_for_current_state();
			}
			break;

		case BME280_STATE_READOUT:
			bme280_powersave();

			{ // convert the readings
				int32_t press_raw =
					((int32_t)(int8_t)m_twi_rx_buf[0] << 12) // press_msb (0xF7)
					| ((int32_t)m_twi_rx_buf[1] << 4)        // press_lsb (0xF8)
					| ((int32_t)m_twi_rx_buf[2] >> 4);       // press_xlsb (0xF9)

				int32_t temp_raw =
					((int32_t)(int8_t)m_twi_rx_buf[3] << 12) // temp_msb (0xFA)
					| ((int32_t)m_twi_rx_buf[4] << 4)        // temp_lsb (0xFB)
					| ((int32_t)m_twi_rx_buf[5] >> 4);       // temp_xlsb (0xFC)

				int32_t hum_raw =
					((int32_t)(int8_t)m_twi_rx_buf[6] << 8) // hum_msb (0xFD)
					| (int32_t)m_twi_rx_buf[7];            // hum_lsb (0xFE)

				m_pressure = bme280_comp_pressure(press_raw);
				m_temperature = bme280_comp_temperature(temp_raw);
				m_humidity = bme280_comp_humidity(hum_raw);
			}

			m_state = BME280_STATE_INITIALIZED;
			m_callback(BME280_EVT_READOUT_COMPLETE);
			break;

		default:
			bme280_powersave();
			return NRF_ERROR_INVALID_STATE;
	}

	return NRF_SUCCESS;
}


static void cb_twim(nrfx_twim_evt_t const * p_event, void * p_context)
{
	NRF_LOG_INFO("cb_twim() called in state %s: event %d", BME280_STATE_NAMES[m_state], p_event->type);

	switch(p_event->type) {
		case NRFX_TWIM_EVT_ADDRESS_NACK:
			m_state = BME280_STATE_NOT_PRESENT;
			bme280_powersave();
			m_callback(BME280_EVT_INIT_NOT_PRESENT);
			break;

		case NRFX_TWIM_EVT_DATA_NACK:
		case NRFX_TWIM_EVT_OVERRUN:
		case NRFX_TWIM_EVT_BUS_ERROR:
			m_state = BME280_STATE_COMMUNICATION_ERROR;
			bme280_powersave();
			m_callback(BME280_EVT_COMMUNICATION_ERROR);
			break;

		case NRFX_TWIM_EVT_DONE:
			APP_ERROR_CHECK(handle_completed_transfer(&p_event->xfer_desc));
			break;
	}
}


static void cb_delay_timer(void *ctx)
{
	APP_ERROR_CHECK(start_transfer_for_current_state());
}


static ret_code_t setup_twim(void)
{
	const nrfx_twim_config_t twim_config = {
		.frequency = NRFX_TWIM_DEFAULT_CONFIG_FREQUENCY,
		.interrupt_priority = NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY,
		.hold_bus_uninit = NRFX_TWIM_DEFAULT_CONFIG_HOLD_BUS_UNINIT,
		.sda = PIN_BME280_SDA,
		.scl = PIN_BME280_SCL};

	ret_code_t err_code = nrfx_twim_init(&m_twim, &twim_config, cb_twim, NULL);
	VERIFY_SUCCESS(err_code);

	nrfx_twim_enable(&m_twim);
	return NRF_SUCCESS;
}


ret_code_t bme280_init(bme280_callback_t callback)
{
	ret_code_t err_code;

	NRF_LOG_INFO("BME280 initializing.");

	err_code = app_timer_create(&m_delay_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_delay_timer);
	VERIFY_SUCCESS(err_code);

	periph_pwr_start_activity(PERIPH_PWR_FLAG_BME280);

	m_callback = callback;

	err_code = setup_twim();
	VERIFY_SUCCESS(err_code);

	m_state = BME280_STATE_READ_CHIPID;
	err_code = app_timer_start(m_delay_timer, APP_TIMER_TICKS(5), NULL);
	return err_code;
}


ret_code_t bme280_start_readout(void)
{
	ret_code_t err_code;

	if(m_state != BME280_STATE_INITIALIZED) {
		return NRF_ERROR_INVALID_STATE;
	}

	periph_pwr_start_activity(PERIPH_PWR_FLAG_BME280);

	err_code = setup_twim();
	VERIFY_SUCCESS(err_code);

	m_state = BME280_STATE_START_MEASUREMENT;
	err_code = app_timer_start(m_delay_timer, APP_TIMER_TICKS(5), NULL);
	return err_code;
}


bool bme280_is_present(void)
{
	return (m_state != BME280_STATE_NOT_PRESENT)
		&& (m_state != BME280_STATE_COMMUNICATION_ERROR);
}


bool bme280_is_ready(void)
{
	return (m_state == BME280_STATE_INITIALIZED);
}


void bme280_powersave(void)
{
	nrfx_twim_disable(&m_twim);
	nrfx_twim_uninit(&m_twim);

	periph_pwr_stop_activity(PERIPH_PWR_FLAG_BME280);
}


float bme280_get_temperature(void)
{
	return m_temperature;
}


float bme280_get_humidity(void)
{
	return m_humidity;
}


float bme280_get_pressure(void)
{
	return m_pressure;
}
