// Driver for the e-Paper-Display on the T-Echo.
// Model: GDEH0154D67
// Controller: SSD1681

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <nrfx_spim.h>
#include <nrf_gpio.h>
#include <sdk_macros.h>
#include <app_timer.h>
#include <nrf_log.h>

#include "pinout.h"
#include "periph_pwr.h"

#include "epaper.h"


#define EPD_MAX_COMMAND_LEN 5

typedef struct
{
	uint8_t config;
	uint8_t data[EPD_MAX_COMMAND_LEN];
} epd_ctrl_entry_t;

#define WAIT_BUSY      0x80  // wait until the busy signal is released after this command
#define SEND_FRAMEBUF  0x40  // send the framebuffer after this command
#define DELAY_10MS     0x20  // wait for 10 milliseconds after sending this command
#define LEN(x)         ((x) & 0x1F)

// Sequence for a full update. The display will be in deep sleep afterwards and
// will require a hardware reset.
const epd_ctrl_entry_t FULL_UPDATE_SEQUENCE[] = {
	{LEN(1) | DELAY_10MS, {0x12}}, // soft reset + startup delay
	{LEN(4),              {0x01, 0xC7, 0x00, 0x00}}, // Driver output control
	{LEN(2),              {0x3C, 0x05}}, // Border Waveform
	{LEN(2),              {0x18, 0x80}}, // Set temp sensor to built-in

	// set RAM area for 200x200 px at offset (0,0)
	{LEN(2),              {0x11, 0x03}}, // Set RAM entry mode: x and y increment, update x after RAM data write
	{LEN(3),              {0x44, 0 / 8, (0 + EPAPER_HEIGHT - 1) / 8}}, // Set RAM x address, start and end
	{LEN(5),              {0x45, 0 % 256, 0 / 256, (0 + EPAPER_WIDTH - 1) % 256, (0 + EPAPER_WIDTH - 1) / 256}}, // Set RAM y address, start and end
	{LEN(2),              {0x4e, 0 / 8}}, // Set RAM x address counter initial value
	{LEN(3),              {0x4f, 0 % 256, 0 / 256}}, // Set RAM y address counter initial value

	// power on
	{LEN(2),              {0x22, 0xF8}},
	{LEN(1) | WAIT_BUSY,  {0x20}},

	// send the new data, twice for full refresh. Due to the size of the image
	// buffer, the data field is not used regularily here and therefore LEN =
	// 0. However, the first data byte specifies the command to use.
	{LEN(0) | SEND_FRAMEBUF, {0x26}},  // previous image
	{LEN(0) | SEND_FRAMEBUF, {0x24}},  // current image

	{LEN(2),              {0x22, 0xF4}},
	{LEN(1) | WAIT_BUSY,  {0x20}},

	// power off and enter deep sleep
	{LEN(2),              {0x22, 0x83}},
	{LEN(1) | WAIT_BUSY,  {0x20}},

	{LEN(2),              {0x10, 0x01}},   // enter deep sleep mode
};


typedef struct
{
	uint8_t x;
	uint8_t y;
} point_t;


typedef enum
{
	TIM_RESET,
	TIM_SEQ_DELAY,
	TIM_WAIT_BUSY,
} timer_state_t;


static nrfx_spim_t m_spim = NRFX_SPIM_INSTANCE(3);

#define FRAMEBUFFER_SIZE_BITS   (EPAPER_WIDTH * EPAPER_HEIGHT)
#define FRAMEBUFFER_SIZE_BYTES  (FRAMEBUFFER_SIZE_BITS / 8)

static uint8_t  m_frame_command[FRAMEBUFFER_SIZE_BYTES + 1];
static uint8_t *m_frame_buffer = m_frame_command+1;

static const epd_ctrl_entry_t *m_seq_ptr;
static const epd_ctrl_entry_t *m_seq_end; // points to the first location beyond the end of the sequence

APP_TIMER_DEF(m_sequence_timer);
static timer_state_t m_timer_state;

#define RESET_ASSERT_TICKS  APP_TIMER_TICKS(1)   // time that the reset is low
#define RESET_DELAY_TICKS   APP_TIMER_TICKS(10)  // time to wait after reset is released
#define BUSY_CHECK_TICKS    APP_TIMER_TICKS(20)  // time between polls of the BUSY signal

static bool m_busy;

static point_t m_cursor;


static void reset_gpios()
{
	nrf_gpio_cfg_default(PIN_EPD_CS);
	nrf_gpio_cfg_default(PIN_EPD_MISO);
	nrf_gpio_cfg_default(PIN_EPD_MOSI);
	nrf_gpio_cfg_default(PIN_EPD_SCK);
	nrf_gpio_cfg_default(PIN_EPD_DC);
}


static ret_code_t send_command(void)
{
	// EasyDMA transfer buffer must reside in RAM, so we copy the constant
	// flash data partially into this buffer.
	static uint8_t bytes2transfer[EPD_MAX_COMMAND_LEN];

	if(m_seq_ptr == m_seq_end) {
		NRF_LOG_INFO("epd: end of sequence.");

		nrfx_spim_uninit(&m_spim); // to save power

		// reset all SPI I/Os to the default state (input). Otherwise a lot of
		// current (~10 mA) flows through the protection diodes of the display once
		// the supply voltage is switched off.
		reset_gpios();

		periph_pwr_stop_activity(PERIPH_PWR_FLAG_EPAPER_UPDATE);

		m_busy = false;

		return NRF_SUCCESS;
	}

	// set up and start SPI transfer from m_seq_ptr
	if(m_seq_ptr->config & SEND_FRAMEBUF) {
		// the framebuffer handled specially, because it is very large and
		// resides in RAM anyway.

		// set the command byte from the sequence entry.
		m_frame_command[0] = m_seq_ptr->data[0];

		// send the complete memory write command
		nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(
				m_frame_command, sizeof(m_frame_command));

		NRF_LOG_INFO("epd: sending framebuffer (cmd: 0x%02x, length: %d).", m_frame_command[0], xfer_desc.tx_length);

		return nrfx_spim_xfer_dcx(&m_spim, &xfer_desc, 0, 1);
	} else {
		uint8_t length = m_seq_ptr->config & 0x1F;

		// make sure the data bytes are in RAM for EasyDMA
		memcpy(bytes2transfer, m_seq_ptr->data, length);
		nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(
				&bytes2transfer, length);

		NRF_LOG_INFO("epd: sending command (cmd: 0x%02x, length: %d).", bytes2transfer[0], xfer_desc.tx_length);

		return nrfx_spim_xfer_dcx(&m_spim, &xfer_desc, 0, 1); // always one command byte
	}
}


static void cb_spim(nrfx_spim_evt_t const *p_event, void *p_context)
{
	const epd_ctrl_entry_t *cur_command = m_seq_ptr;
	m_seq_ptr++;

	NRF_LOG_INFO("epd: SPIM transfer finished.");

	// check special post-processing flags
	if(cur_command->config & DELAY_10MS) {
		NRF_LOG_INFO("epd: starting delay.");
		m_timer_state = TIM_SEQ_DELAY;
		APP_ERROR_CHECK(app_timer_start(m_sequence_timer, RESET_DELAY_TICKS, NULL));
	} else if(cur_command->config & WAIT_BUSY) {
		NRF_LOG_INFO("epd: starting wait for BUSY.");
		m_timer_state = TIM_WAIT_BUSY;
		APP_ERROR_CHECK(app_timer_start(m_sequence_timer, BUSY_CHECK_TICKS, NULL));
	} else {
		NRF_LOG_INFO("epd: directly starting next transfer.");
		// directly execute the next command

		// note: direct start seems to be too fast, so we add 10 ms delay before sending the next command
		m_timer_state = TIM_SEQ_DELAY;
		APP_ERROR_CHECK(app_timer_start(m_sequence_timer, APP_TIMER_TICKS(10), NULL));
		//send_command();
	}

}


static void cb_sequence_timer(void *p_context)
{
	switch(m_timer_state)
	{
		case TIM_RESET:
			NRF_LOG_INFO("epd: reset finished.");

			// reset has been asserted before this timer interval
			nrf_gpio_cfg_input(PIN_EPD_RST, NRF_GPIO_PIN_PULLUP); // clear the reset

			// wait a bit and start the sequence then
			m_timer_state = TIM_SEQ_DELAY;
			APP_ERROR_CHECK(app_timer_start(m_sequence_timer, RESET_DELAY_TICKS, NULL));
			break;

		case TIM_SEQ_DELAY:
			// a delay in the sequence has finished => send the next command
			NRF_LOG_INFO("epd: delay timer finished.");
			APP_ERROR_CHECK(send_command());
			break;

		case TIM_WAIT_BUSY:
			if(nrf_gpio_pin_read(PIN_EPD_BUSY)) {
				// still busy, restart the timer
				NRF_LOG_INFO("epd: still busy, retrying.");
				APP_ERROR_CHECK(app_timer_start(m_sequence_timer, BUSY_CHECK_TICKS, NULL));
			} else {
				// not busy anymore => send the next command
				NRF_LOG_INFO("epd: busy flag released.");
				APP_ERROR_CHECK(send_command());
			}
			break;
	}
}


ret_code_t epaper_init(void)
{
	// initialize the GPIOs.
	nrf_gpio_cfg_input(PIN_EPD_RST, NRF_GPIO_PIN_PULLUP);

	nrf_gpio_cfg_input(PIN_EPD_BUSY, NRF_GPIO_PIN_NOPULL);

	//nrf_gpio_pin_set(PIN_EPD_BL);
	//nrf_gpio_cfg_output(PIN_EPD_BL);

	// SPI will be initialized on demand. To prevent spurious driver
	// activation, apply a pullup on the CS pin.
	nrf_gpio_cfg_input(PIN_EPD_CS, NRF_GPIO_PIN_PULLUP);

	m_frame_command[0] = 0x24; // write B/W RAM command

	epaper_fb_clear(EPAPER_COLOR_WHITE);
	m_cursor.x = m_cursor.y = 0;

	NRF_LOG_INFO("epd: init.");

	return app_timer_create(&m_sequence_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_sequence_timer);
}


ret_code_t epaper_update(void)
{
	if(m_busy) {
		return NRF_ERROR_BUSY;
	}

	periph_pwr_start_activity(PERIPH_PWR_FLAG_EPAPER_UPDATE);

	nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
	spi_config.frequency      = NRF_SPIM_FREQ_8M;
	spi_config.ss_pin         = PIN_EPD_CS;
	spi_config.miso_pin       = PIN_EPD_MISO;
	spi_config.mosi_pin       = PIN_EPD_MOSI;
	spi_config.sck_pin        = PIN_EPD_SCK;
	spi_config.dcx_pin        = PIN_EPD_DC;

	VERIFY_SUCCESS(nrfx_spim_init(&m_spim, &spi_config, cb_spim, NULL));

	// send the power-on sequence after asserting the hardware reset
	m_seq_ptr = FULL_UPDATE_SEQUENCE;
	m_seq_end = FULL_UPDATE_SEQUENCE + (sizeof(FULL_UPDATE_SEQUENCE) / sizeof(FULL_UPDATE_SEQUENCE[0]));

	// assert the hardware reset
	nrf_gpio_pin_clear(PIN_EPD_RST);
	nrf_gpio_cfg_output(PIN_EPD_RST);

	NRF_LOG_INFO("epd: starting update sequence.");

	m_timer_state = TIM_RESET;
	VERIFY_SUCCESS(app_timer_start(m_sequence_timer, RESET_ASSERT_TICKS, NULL));

	// the rest will happen asynchronously. See cb_sequence_timer() and cb_spim().
	m_busy = true;

	return NRF_SUCCESS;
}


void epaper_fb_clear(uint8_t color)
{
	if(color) {
		memset(m_frame_buffer, 0xFF, FRAMEBUFFER_SIZE_BYTES);
	} else {
		memset(m_frame_buffer, 0x00, FRAMEBUFFER_SIZE_BYTES);
	}
}


void epaper_fb_set_pixel(uint8_t x, uint8_t y, uint8_t color)
{
	// adressing scheme is: first down (LSB first) then right.
	uint32_t bitidx = x * EPAPER_HEIGHT + y;

	if(color) {
		m_frame_buffer[bitidx / 8] |= (1 << (7 - bitidx % 8));
	} else {
		m_frame_buffer[bitidx / 8] &= ~(1 << (7 - bitidx % 8));
	}
}


void epaper_fb_move_to(uint8_t x, uint8_t y)
{
	m_cursor.x = x;
	m_cursor.y = y;
}


/* Line-drawing using the Bresenham algorithm. */
void epaper_fb_line_to(uint8_t xe, uint8_t ye, uint8_t color)
{
	bool flip_xy = false; // mirror on the 45°-axis
	bool neg_x = false; // line moves leftwards (to smaller x)
	bool neg_y = false; // line moves upwards (to smaller y)

	uint8_t xa = m_cursor.x;
	uint8_t ya = m_cursor.y;

	uint8_t x = 0;
	uint8_t y = 0;

	int16_t dx = (int16_t)xe - (int16_t)xa;
	int16_t dy = (int16_t)ye - (int16_t)ya;

	if(abs(dy) > abs(dx)) {
		dx = (int16_t)ye - (int16_t)ya;
		dy = (int16_t)xe - (int16_t)xa;
		flip_xy = true;
	}

	if(dx < 0) {
		neg_x = true;
		dx = -dx;
	}

	if(dy < 0) {
		neg_y = true;
		dy = -dy;
	}

	int16_t d    = 2*dy - dx;

	int16_t d_o  = 2*dy;
	int16_t d_no = 2*(dy - dx);

	while(x <= dx) {
		int16_t tx = neg_x ? -x : x;
		int16_t ty = neg_y ? -y : y;

		if(flip_xy) {
			epaper_fb_set_pixel(xa + ty, ya + tx, color);
		} else {
			epaper_fb_set_pixel(xa + tx, ya + ty, color);
		}

		x++;

		if(d <= 0) {
			d += d_o;
		} else {
			d += d_no;
			y++;
		}
	}

	m_cursor.x = xe;
	m_cursor.y = ye;
}
