/*
 * vim: noexpandtab
 *
 * Copyright (c) 2021-2022 Thomas Kolb <cfr34k-git@tkolb.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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

#define NRF_LOG_MODULE_NAME epd
#define NRF_LOG_LEVEL NRF_LOG_SEVERITY_INFO
#include <nrf_log.h>
NRF_LOG_MODULE_REGISTER();

#include "pinout.h"
#include "periph_pwr.h"
#include "fasttrigon.h"

#include "epaper.h"


#define EPD_MAX_COMMAND_LEN 5

typedef struct
{
	uint8_t config;
	uint8_t data[EPD_MAX_COMMAND_LEN];
} epd_ctrl_entry_t;

#define WAIT_BUSY           0x80  // wait until the busy signal is released after this command
#define SEND_FRAMEBUF       0x40  // send the framebuffer after this command
#define DELAY_10MS          0x20  // wait for 10 milliseconds after sending this command
#define SEND_FRAMEBUF_PREV  0x10  // send the previous framebuffer after this command
#define LEN(x)              ((x) & 0x0F)

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

	// send the new data, twice for full refresh. Due to the size of the image
	// buffer, the data field is not used regularily here and therefore LEN =
	// 0. However, the first data byte specifies the command to use.
	{LEN(0) | SEND_FRAMEBUF, {0x26}},  // previous image
	{LEN(0) | SEND_FRAMEBUF, {0x24}},  // current image

	{LEN(2),              {0x22, 0xF7}}, // full update
	{LEN(1) | WAIT_BUSY,  {0x20}},

	// enter deep sleep
	{LEN(2),              {0x10, 0x01}},   // enter deep sleep mode
};


// Sequence for a partial update. The display will be in deep sleep afterwards and
// will require a hardware reset.
const epd_ctrl_entry_t PARTIAL_UPDATE_SEQUENCE[] = {
	{LEN(1) | DELAY_10MS, {0x12}}, // soft reset + startup delay
	{LEN(4),              {0x01, 0xC7, 0x00, 0x00}}, // Driver output control
	{LEN(2),              {0x3C, 0x80}}, // Border Waveform
	{LEN(2),              {0x18, 0x80}}, // Set temp sensor to built-in

	// set RAM area for 200x200 px at offset (0,0)
	{LEN(2),              {0x11, 0x03}}, // Set RAM entry mode: x and y increment, update x after RAM data write
	{LEN(3),              {0x44, 0 / 8, (0 + EPAPER_HEIGHT - 1) / 8}}, // Set RAM x address, start and end
	{LEN(5),              {0x45, 0 % 256, 0 / 256, (0 + EPAPER_WIDTH - 1) % 256, (0 + EPAPER_WIDTH - 1) / 256}}, // Set RAM y address, start and end
	{LEN(2),              {0x4e, 0 / 8}}, // Set RAM x address counter initial value
	{LEN(3),              {0x4f, 0 % 256, 0 / 256}}, // Set RAM y address counter initial value

	// Load LUT
	//{LEN(2),              {0x22, 0xB9}},
	//{LEN(1) | WAIT_BUSY,  {0x20}},

	// send the new image. Due to the size of the image
	// buffer, the data field is not used regularily here and therefore LEN =
	// 0. However, the first data byte specifies the command to use.
	{LEN(0) | SEND_FRAMEBUF_PREV, {0x26}},  // previous image
	{LEN(0) | SEND_FRAMEBUF     , {0x24}},  // current image

	{LEN(2),              {0x22, 0xFF}}, // partial update
	{LEN(1) | WAIT_BUSY,  {0x20}},

	// enter deep sleep
	{LEN(2),              {0x10, 0x01}},   // enter deep sleep mode
};


typedef struct
{
	uint8_t x;
	uint8_t y;
} point_t;


typedef enum
{
	TIM_STARTUP,
	TIM_RESET,
	TIM_SEQ_DELAY,
	TIM_WAIT_BUSY,
} timer_state_t;


typedef enum
{
	SPI_CMD,
	SPI_DATA,
} spi_state_t;


static nrfx_spim_t m_spim = NRFX_SPIM_INSTANCE(0);

#define FRAMEBUFFER_SIZE_BITS   (EPAPER_WIDTH * EPAPER_HEIGHT)
#define FRAMEBUFFER_SIZE_BYTES  (FRAMEBUFFER_SIZE_BITS / 8)

static uint8_t  m_frame_command[FRAMEBUFFER_SIZE_BYTES + 1];
static uint8_t *m_frame_buffer = m_frame_command+1;

static uint8_t  m_frame_command_prev[FRAMEBUFFER_SIZE_BYTES + 1];
static uint8_t *m_frame_buffer_prev = m_frame_command_prev+1;

static const epd_ctrl_entry_t *m_seq_ptr;
static const epd_ctrl_entry_t *m_seq_end; // points to the first location beyond the end of the sequence

static uint8_t    *m_spi_data;
static uint16_t    m_spi_data_len;
static spi_state_t m_spi_state;

APP_TIMER_DEF(m_sequence_timer);
static timer_state_t m_timer_state;

#define STARTUP_TICKS       APP_TIMER_TICKS(10)  // time to wait after applying power
#define RESET_ASSERT_TICKS  APP_TIMER_TICKS(20)  // time that the reset is low
#define RESET_DELAY_TICKS   APP_TIMER_TICKS(10)  // time to wait after reset is released
#define BUSY_CHECK_TICKS    APP_TIMER_TICKS(20)  // time between polls of the BUSY signal

static uint16_t m_busy_check_counter = 0;

static bool m_busy;
static bool m_shutdown_needed;

static point_t m_cursor;
static const GFXfont *m_font;


static ret_code_t send_command(void)
{
	// EasyDMA transfer buffer must reside in RAM, so we copy the constant
	// flash data partially into this buffer.
	static uint8_t bytes2transfer[EPD_MAX_COMMAND_LEN];

	if(m_seq_ptr == m_seq_end) {
		NRF_LOG_DEBUG("end of sequence.");

		// actual display shutdown will be done from the main loop
		m_shutdown_needed = true;

		return NRF_SUCCESS;
	}

	m_spi_state = SPI_CMD;

	// set up DC and CS pins
	nrf_gpio_pin_clear(PIN_EPD_DC); // 0 => command
	nrf_gpio_pin_clear(PIN_EPD_CS);

	// set up and start SPI transfer from m_seq_ptr
	if(m_seq_ptr->config & SEND_FRAMEBUF) {
		// the framebuffer handled specially, because it is very large and
		// resides in RAM anyway.

		// set the command byte from the sequence entry.
		m_frame_command[0] = m_seq_ptr->data[0];

		// prepare the data and size
		m_spi_data     = m_frame_buffer;
		m_spi_data_len = FRAMEBUFFER_SIZE_BYTES;

		// send the frame command
		nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(
				m_frame_command, 1);

		NRF_LOG_DEBUG("sending framebuffer (cmd: 0x%02x, length: %d).", m_frame_command[0], xfer_desc.tx_length);

		return nrfx_spim_xfer(&m_spim, &xfer_desc, 0);
	} else if(m_seq_ptr->config & SEND_FRAMEBUF_PREV) {
		// same as for the framebuffer above, but with the previous image buffer

		// set the command byte from the sequence entry.
		m_frame_command_prev[0] = m_seq_ptr->data[0];

		// prepare the data and size
		m_spi_data     = m_frame_buffer_prev;
		m_spi_data_len = FRAMEBUFFER_SIZE_BYTES;

		// send the frame command
		nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(
				m_frame_command_prev, 1);

		NRF_LOG_DEBUG("sending previous framebuffer (cmd: 0x%02x, length: %d).", m_frame_command_prev[0], xfer_desc.tx_length);

		return nrfx_spim_xfer(&m_spim, &xfer_desc, 0);
	} else {
		uint8_t length = m_seq_ptr->config & 0x1F;

		// make sure the data bytes are in RAM for EasyDMA
		memcpy(bytes2transfer, m_seq_ptr->data, length);

		// prepare the data and size
		m_spi_data     = bytes2transfer + 1;
		m_spi_data_len = length - 1;

		nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(
				bytes2transfer, 1);

		NRF_LOG_DEBUG("sending command (cmd: 0x%02x, length: %d).", bytes2transfer[0], xfer_desc.tx_length);

		return nrfx_spim_xfer(&m_spim, &xfer_desc, 0); // always one command byte
	}
}


static void cb_spim(nrfx_spim_evt_t const *p_event, void *p_context)
{
	const epd_ctrl_entry_t *cur_command = m_seq_ptr;

	NRF_LOG_DEBUG("SPIM transfer finished.");

	if(m_spi_state == SPI_CMD && m_spi_data_len != 0) {
		// send the data
		nrf_gpio_pin_set(PIN_EPD_DC); // 1 => data

		m_spi_state = SPI_DATA;

		NRF_LOG_DEBUG("sending %d data bytes.", m_spi_data_len);

		nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(
				m_spi_data, m_spi_data_len);

		APP_ERROR_CHECK(nrfx_spim_xfer(&m_spim, &xfer_desc, 0));
	} else {
		nrf_gpio_pin_set(PIN_EPD_CS); // command complete

		m_seq_ptr++; // go to next command

		// check special post-processing flags
		if(cur_command->config & DELAY_10MS) {
			NRF_LOG_DEBUG("starting delay.");
			m_timer_state = TIM_SEQ_DELAY;
			APP_ERROR_CHECK(app_timer_start(m_sequence_timer, RESET_DELAY_TICKS, NULL));
		} else if(cur_command->config & WAIT_BUSY) {
			NRF_LOG_DEBUG("starting wait for BUSY.");
			m_timer_state = TIM_WAIT_BUSY;
			APP_ERROR_CHECK(app_timer_start(m_sequence_timer, BUSY_CHECK_TICKS, NULL));
		} else {
			NRF_LOG_DEBUG("directly starting next transfer.");
			// directly execute the next command

			// note: direct start seems to be too fast, so we add 10 ms delay before sending the next command
			send_command();
		}
	}

}


static void cb_sequence_timer(void *p_context)
{
	switch(m_timer_state)
	{
		case TIM_STARTUP:
			NRF_LOG_DEBUG("startup finished.");

			// assert the hardware reset
			nrf_gpio_pin_clear(PIN_EPD_RST);
			nrf_gpio_cfg_output(PIN_EPD_RST);

			// hold the reset for some time
			m_timer_state = TIM_RESET;
			APP_ERROR_CHECK(app_timer_start(m_sequence_timer, RESET_ASSERT_TICKS, NULL));
			break;

		case TIM_RESET:
			NRF_LOG_DEBUG("reset finished.");

			// reset has been asserted before this timer interval
			nrf_gpio_pin_set(PIN_EPD_RST);
			nrf_gpio_cfg_input(PIN_EPD_RST, NRF_GPIO_PIN_PULLUP); // clear the reset

			// wait a bit and start the sequence then
			m_timer_state = TIM_SEQ_DELAY;
			APP_ERROR_CHECK(app_timer_start(m_sequence_timer, RESET_DELAY_TICKS, NULL));
			break;

		case TIM_SEQ_DELAY:
			// a delay in the sequence has finished => send the next command
			NRF_LOG_DEBUG("delay timer finished.");
			APP_ERROR_CHECK(send_command());
			break;

		case TIM_WAIT_BUSY:
			if(nrf_gpio_pin_read(PIN_EPD_BUSY)) {
				// still busy, restart the timer
				m_busy_check_counter++;
				APP_ERROR_CHECK(app_timer_start(m_sequence_timer, BUSY_CHECK_TICKS, NULL));
			} else {
				// not busy anymore => send the next command
				NRF_LOG_DEBUG("busy flag released after %d polls.", m_busy_check_counter);
				m_busy_check_counter = 0;
				APP_ERROR_CHECK(send_command());
			}
			break;
	}
}


void epaper_config_gpios(bool power_supplied)
{
	nrf_gpio_cfg_default(PIN_EPD_MISO);
	nrf_gpio_cfg_default(PIN_EPD_MOSI);
	nrf_gpio_cfg_default(PIN_EPD_SCK);
	nrf_gpio_cfg_default(PIN_EPD_DC);

	if(power_supplied) {
		// while power is on, it is better to keep the pullups on Chip Select and
		// the Reset pin to prevent noise causing spurious commands to be executed.
		nrf_gpio_cfg_input(PIN_EPD_CS, NRF_GPIO_PIN_PULLUP);
		nrf_gpio_cfg_input(PIN_EPD_RST, NRF_GPIO_PIN_PULLUP);
	} else {
		// reset all SPI I/Os to the default state (input). Otherwise a lot of
		// current (~10 mA) flows through the protection diodes of the display once
		// the supply voltage is switched off.
		nrf_gpio_cfg_default(PIN_EPD_CS);
		nrf_gpio_cfg_default(PIN_EPD_RST);
	}
}


ret_code_t epaper_init(void)
{
	// initialize the GPIOs.
	nrf_gpio_cfg_default(PIN_EPD_RST);
	nrf_gpio_cfg_input(PIN_EPD_BUSY, NRF_GPIO_PIN_NOPULL);

	//nrf_gpio_pin_set(PIN_EPD_BL);
	//nrf_gpio_cfg_output(PIN_EPD_BL);

	// SPI will be initialized on demand. To prevent spurious driver
	// activation, apply a pullup on the CS pin.
	nrf_gpio_cfg_default(PIN_EPD_CS);

	m_frame_command[0] = 0x24; // write B/W RAM command
	m_frame_command_prev[0] = 0x26; // write B/W RAM command (previous image)

	epaper_fb_clear(EPAPER_COLOR_WHITE);

	m_cursor.x = m_cursor.y = 0;
	m_font = NULL;

	NRF_LOG_DEBUG("init.");

	return app_timer_create(&m_sequence_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_sequence_timer);
}


ret_code_t epaper_update(bool full_refresh)
{
	if(m_busy) {
		return NRF_ERROR_BUSY;
	}

	periph_pwr_start_activity(PERIPH_PWR_FLAG_EPAPER_UPDATE);

	nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
	spi_config.frequency      = NRF_SPIM_FREQ_8M;
	spi_config.ss_pin         = NRFX_SPIM_PIN_NOT_USED; // CS is controlled manually
	spi_config.miso_pin       = PIN_EPD_MISO;
	spi_config.mosi_pin       = PIN_EPD_MOSI;
	spi_config.sck_pin        = PIN_EPD_SCK;

	VERIFY_SUCCESS(nrfx_spim_init(&m_spim, &spi_config, cb_spim, NULL));

	// according to the Devzone, SPI at 8 MHz requires high drive outputs
	nrf_gpio_pin_set(PIN_EPD_CS);
	nrf_gpio_pin_clear(PIN_EPD_DC);

	nrf_gpio_cfg(PIN_EPD_CS,   NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(PIN_EPD_MOSI, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(PIN_EPD_SCK,  NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg(PIN_EPD_DC,   NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);

	// send the power-on sequence after asserting the hardware reset
	if(full_refresh) {
		m_seq_ptr = FULL_UPDATE_SEQUENCE;
		m_seq_end = FULL_UPDATE_SEQUENCE + (sizeof(FULL_UPDATE_SEQUENCE) / sizeof(FULL_UPDATE_SEQUENCE[0]));
	} else {
		m_seq_ptr = PARTIAL_UPDATE_SEQUENCE;
		m_seq_end = PARTIAL_UPDATE_SEQUENCE + (sizeof(PARTIAL_UPDATE_SEQUENCE) / sizeof(PARTIAL_UPDATE_SEQUENCE[0]));
	}

	nrf_gpio_cfg_input(PIN_EPD_RST, NRF_GPIO_PIN_PULLUP);

	NRF_LOG_DEBUG("starting update sequence.");

	m_timer_state = TIM_STARTUP;
	VERIFY_SUCCESS(app_timer_start(m_sequence_timer, STARTUP_TICKS, NULL));

	// the rest will happen asynchronously. See cb_sequence_timer() and cb_spim().
	m_busy = true;
	m_shutdown_needed = false;

	return NRF_SUCCESS;
}


bool epaper_is_busy(void)
{
	return m_busy;
}


void epaper_loop(void)
{
	if(m_shutdown_needed) {
		nrfx_spim_uninit(&m_spim); // to save power

		epaper_config_gpios(true); // safe powered state

		periph_pwr_stop_activity(PERIPH_PWR_FLAG_EPAPER_UPDATE);

		// copy last sent framebuffer to previous image buffer
		memcpy(m_frame_buffer_prev, m_frame_buffer, FRAMEBUFFER_SIZE_BYTES);

		m_busy = false;
		m_shutdown_needed = false;
	}
}


/***** Framebuffer drawing functions *****/

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
	if(x >= EPAPER_WIDTH || y >= EPAPER_HEIGHT) {
		return;
	}

	// adressing scheme is: first down (LSB first) then left.
	uint32_t bitidx = (EPAPER_WIDTH - x - 1) * EPAPER_HEIGHT + y;

	if(color & EPAPER_COLOR_MASK) {
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
	static uint16_t pixcount = 0;

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

	uint8_t line_drawing_mode = color & EPAPER_LINE_DRAWING_MODE_MASK;

	while(x <= dx) {
		int16_t tx = neg_x ? -x : x;
		int16_t ty = neg_y ? -y : y;

		bool draw_pixel = true;

		if(line_drawing_mode == EPAPER_LINE_DRAWING_MODE_DASHED) {
			draw_pixel = (pixcount % 8) < 5;
		} else if(line_drawing_mode == EPAPER_LINE_DRAWING_MODE_DOTTED_LIGHT) {
			draw_pixel = (pixcount % 3) == 0;
		} else if(line_drawing_mode == EPAPER_LINE_DRAWING_MODE_DOTTED) {
			draw_pixel = (pixcount % 2) == 0;
		}

		if(draw_pixel) {
			if(flip_xy) {
				epaper_fb_set_pixel(xa + ty, ya + tx, color);
			} else {
				epaper_fb_set_pixel(xa + tx, ya + ty, color);
			}
		}

		x++;

		if(d <= 0) {
			d += d_o;
		} else {
			d += d_no;
			y++;
		}

		pixcount++;
	}

	m_cursor.x = xe;
	m_cursor.y = ye;
}

void epaper_fb_circle(uint8_t radius, uint8_t color)
{
	point_t m_center = m_cursor;

	uint32_t npoints = 3UL * radius;

	epaper_fb_move_to(m_center.x + radius, m_center.y);

	for(uint32_t i = 0; i < npoints; i++) {
		int32_t delta_x = radius * fasttrigon_cos(i * FASTTRIGON_LUT_SIZE / npoints) / FASTTRIGON_SCALE;
		int32_t delta_y = radius * fasttrigon_sin(i * FASTTRIGON_LUT_SIZE / npoints) / FASTTRIGON_SCALE;

		epaper_fb_line_to((int32_t)m_center.x + delta_x, (int32_t)m_center.y + delta_y, color);
	}

	m_cursor = m_center;
}

void epaper_fb_draw_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color)
{
	epaper_fb_move_to(left, bottom);
	epaper_fb_line_to(right, bottom, color);
	epaper_fb_line_to(right, top, color);
	epaper_fb_line_to(left, top, color);
	epaper_fb_line_to(left, bottom, color);
}

void epaper_fb_fill_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color)
{
	for(uint8_t x = left; x <= right; x++) {
		for(uint8_t y = top; y <= bottom; y++) {
			epaper_fb_set_pixel(x, y, color);
		}
	}
}

/* Font-drawing functions: These support drawing text from Adafruit GFX fonts.
 * Use the 'fontconvert' utility from Adafruit GFX to generate fonts:
 * https://github.com/adafruit/Adafruit-GFX-Library/tree/master/fontconvert
 */

void epaper_fb_set_font(const GFXfont *font)
{
	m_font = font;
}

ret_code_t epaper_fb_draw_char(uint8_t c, uint8_t color)
{
	if(!m_font) {
		return NRF_ERROR_INVALID_STATE;
	}

	if(c < m_font->first || c > m_font->last) {
		return NRF_ERROR_INVALID_PARAM;
	}

	GFXglyph *glyph = &m_font->glyph[c - m_font->first];
	uint8_t  *bitmap = m_font->bitmap + glyph->bitmapOffset;

	uint32_t bitidx = 0;
	uint8_t current_byte = 0;

	for(uint32_t y = 0; y < glyph->height; y++) {
		for(uint32_t x = 0; x < glyph->width; x++) {
			if((bitidx & 0x7) == 0) {
				current_byte = bitmap[bitidx / 8];
			}

			if(current_byte & 0x80) {
				epaper_fb_set_pixel(
						m_cursor.x + glyph->xOffset + x,
						m_cursor.y + glyph->yOffset + y,
						color);
			}

			current_byte <<= 1;
			bitidx++;
		}
	}

	m_cursor.x += glyph->xAdvance;

	return NRF_SUCCESS;
}

uint8_t epaper_fb_calc_text_width(const char *s)
{
	uint8_t total_width = 0;

	while(*s) {
		char c = *s;

		// replace non-printable characters
		if(c < m_font->first || c > m_font->last) {
			c = '?';
		}

		GFXglyph *glyph = &m_font->glyph[c - m_font->first];

		total_width += glyph->xAdvance;

		s++;
	}

	return total_width;
}

ret_code_t epaper_fb_draw_string(const char *s, uint8_t color)
{
	while(*s) {
		ret_code_t err_code = epaper_fb_draw_char((uint8_t)*s, color);

		if(err_code == NRF_ERROR_INVALID_PARAM) {
			// this character is not in the font, draw replacement character
			VERIFY_SUCCESS(epaper_fb_draw_char('?', color));
		} else {
			VERIFY_SUCCESS(err_code);
		}

		s++;
	}

	return NRF_SUCCESS;
}

ret_code_t epaper_fb_draw_data_wrapped(const uint8_t *s, size_t len, uint8_t color)
{
	uint8_t start_pos_x = m_cursor.x;

	if(!m_font) {
		return NRF_ERROR_INVALID_STATE;
	}

	for(size_t i = 0; i < len; i++) {
		char c = s[i];

		uint8_t endpos = m_cursor.x;
		if(c >= m_font->first && c <= m_font->last) {
			endpos += m_font->glyph[c - m_font->first].width;
		}

		if(endpos >= EPAPER_WIDTH) {
			m_cursor.x = start_pos_x;
			m_cursor.y += m_font->yAdvance;
		}

		ret_code_t err_code = epaper_fb_draw_char((uint8_t)c, color);

		if(err_code == NRF_ERROR_INVALID_PARAM) {
			// this character is not in the font, draw replacement character
			VERIFY_SUCCESS(epaper_fb_draw_char('?', color));
		} else {
			VERIFY_SUCCESS(err_code);
		}
	}

	return NRF_SUCCESS;
}


ret_code_t epaper_fb_draw_string_wrapped(const char *s, uint8_t color)
{
	return epaper_fb_draw_data_wrapped((const uint8_t*)s, strlen(s), color);
}


uint8_t epaper_fb_get_line_height(void)
{
	if(m_font) {
		return m_font->yAdvance;
	} else {
		return 0;
	}
}

uint8_t epaper_fb_get_cursor_pos_x(void)
{
	return m_cursor.x;
}

uint8_t epaper_fb_get_cursor_pos_y(void)
{
	return m_cursor.y;
}
