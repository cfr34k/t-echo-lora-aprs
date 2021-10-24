// Driver for the e-Paper-Display on the T-Echo.
// Model: GDEH0154D67
// Controller: SSD1681

#include <stdint.h>

#include <nrfx_spim.h>
#include <nrf_gpio.h>
#include <sdk_macros.h>
#include <app_timer.h>

#include "pinout.h"

#include "epaper.h"


typedef struct
{
	uint8_t config;
	uint8_t word;
} epd_ctrl_entry_t;

#define CFG_CMD        0x80
#define CFG_DATA       0x00
#define WAIT_BUSY      0x40  // wait until the busy signal is released after this command
#define SEND_FRAMEBUF  0x20  // send the framebuffer after this command
#define DELAY_10MS     0x08 // wait for 10 milliseconds after sending this command

// Sequence for a full update. The display will be in deep sleep afterwards and
// will require a hardware reset.
const epd_ctrl_entry_t FULL_UPDATE_SEQUENCE[] = {
	{CFG_CMD | DELAY_10MS, 0x12}, // soft reset + startup delay
	{CFG_CMD,  0x01}, // Driver output control
	{CFG_DATA, 0xC7},
	{CFG_DATA, 0x00},
	{CFG_DATA, 0x00},
	{CFG_CMD,  0x3C}, // Border Waveform
	{CFG_DATA, 0x05},
	{CFG_CMD,  0x18}, // Set temp sensor to built-in
	{CFG_DATA, 0x80},

	// set RAM area for 200x200 px at offset (0,0)
	{CFG_CMD,  0x11}, // Set RAM entry mode:
	{CFG_DATA, 0x03}, // x and y increment, update x after RAM data write
	{CFG_CMD,  0x44}, // Set RAM x address, start and end
	{CFG_DATA, 0 / 8},
	{CFG_DATA, (0 + 200 - 1) / 8},
	{CFG_CMD,  0x45}, // Set RAM y address, start and end
	{CFG_DATA, 0 % 256},
	{CFG_DATA, 0 / 256},
	{CFG_DATA, (0 + 200 - 1) % 256},
	{CFG_DATA, (0 + 200 - 1) / 256},
	{CFG_CMD,  0x4e}, // Set RAM x address counter initial value
	{CFG_DATA, 0 / 8},
	{CFG_CMD,  0x4f}, // Set RAM y address counter initial value
	{CFG_DATA, 0 % 256},
	{CFG_DATA, 0 / 256},

	// power on
	{CFG_CMD,  0x22},
	{CFG_DATA, 0xF8},
	{CFG_CMD | WAIT_BUSY, 0x20},

	// send the new data
	{CFG_CMD | SEND_FRAMEBUF, 0x24},

	{CFG_CMD,  0x22},
	{CFG_DATA, 0xF4},
	{CFG_CMD | WAIT_BUSY, 0x20},

	// power off and enter deep sleep
	{CFG_CMD,  0x22},
	{CFG_DATA, 0x83},
	{CFG_CMD | WAIT_BUSY, 0x20},
	{CFG_CMD,  0x10},   // enter deep sleep mode
	{CFG_DATA, 0x01},
};


typedef enum
{
	TIM_RESET,
	TIM_SEQ_DELAY,
	TIM_WAIT_BUSY,
} timer_state_t;



static nrfx_spim_t m_spim = NRFX_SPIM_INSTANCE(0);

static uint8_t m_frame_buffer[200*200/8];
static bool m_framebuf_sent = false;

static const epd_ctrl_entry_t *m_seq_ptr;
static const epd_ctrl_entry_t *m_seq_end; // points to the first location beyond the end of the sequence

APP_TIMER_DEF(m_sequence_timer);
static timer_state_t m_timer_state;

#define RESET_ASSERT_TICKS  APP_TIMER_TICKS(1)   // time that the reset is low
#define RESET_DELAY_TICKS   APP_TIMER_TICKS(10)  // time to wait after reset is released
#define BUSY_CHECK_TICKS    APP_TIMER_TICKS(20)  // time between polls of the BUSY signal

static bool m_busy;


static ret_code_t send_control(void)
{
	static uint8_t byte2transfer;

	if(m_seq_ptr == m_seq_end) {
		nrfx_spim_uninit(&m_spim); // to save power
		m_busy = false;

		return NRF_SUCCESS;
	}

	// set the D/C pin
	if(m_seq_ptr->config & CFG_CMD) {
		nrf_gpio_pin_set(PIN_EPD_DC);
	} else {
		nrf_gpio_pin_clear(PIN_EPD_DC);
	}

	// set up and start SPI transfer from m_seq_ptr
	byte2transfer = m_seq_ptr->word; // must reside in RAM
	nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(
			&byte2transfer, 1);
			

	return nrfx_spim_xfer(&m_spim, &xfer_desc, 0);
}


static void cb_spim(nrfx_spim_evt_t const *p_event, void *p_context)
{
	if(m_seq_ptr->config & 0x7F) {
		// special post-processing operations are requested for this command
		if(m_seq_ptr->config & DELAY_10MS) {
			m_timer_state = TIM_SEQ_DELAY;
			APP_ERROR_CHECK(app_timer_start(m_sequence_timer, RESET_DELAY_TICKS, NULL));
		} else if(m_seq_ptr->config & WAIT_BUSY) {
			m_timer_state = TIM_WAIT_BUSY;
			APP_ERROR_CHECK(app_timer_start(m_sequence_timer, BUSY_CHECK_TICKS, NULL));
		} else if(m_seq_ptr->config & SEND_FRAMEBUF) {
			if(m_framebuf_sent) {
				// returning from the framebuffer transmission
				m_framebuf_sent = false;

				// continue with the sequence
				m_seq_ptr++;
				APP_ERROR_CHECK(send_control());
				return;
			} else {
				// the framebuffer shall be sent now
				m_framebuf_sent = true;

				// send the complete framebuffer as data
				nrf_gpio_pin_clear(PIN_EPD_DC);

				nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(
						m_frame_buffer, sizeof(m_frame_buffer));
						

				APP_ERROR_CHECK(nrfx_spim_xfer(&m_spim, &xfer_desc, 0));
			}
		}

		m_seq_ptr++;
	} else {
		// no special processing necessary => directly continue with the next command
		m_seq_ptr++;
		APP_ERROR_CHECK(send_control());
	}
}


static void cb_sequence_timer(void *p_context)
{
	switch(m_timer_state)
	{
		case TIM_RESET:
			// reset has been asserted before this timer interval
			nrf_gpio_cfg_input(PIN_EPD_RST, NRF_GPIO_PIN_PULLUP); // clear the reset

			// wait a bit and start the sequence then
			m_timer_state = TIM_SEQ_DELAY;
			APP_ERROR_CHECK(app_timer_start(m_sequence_timer, RESET_DELAY_TICKS, NULL));
			break;

		case TIM_SEQ_DELAY:
			// a delay in the sequence has finished => send the next control byte
			APP_ERROR_CHECK(send_control());
			break;

		case TIM_WAIT_BUSY:
			if(nrf_gpio_pin_read(PIN_EPD_BUSY)) {
				// still busy, restart the timer
				APP_ERROR_CHECK(app_timer_start(m_sequence_timer, BUSY_CHECK_TICKS, NULL));
			} else {
				// not busy anymore => send the next control byte
				APP_ERROR_CHECK(send_control());
			}
			break;
	}
}


ret_code_t epaper_init(void)
{
	// initialize the GPIOs.
	nrf_gpio_cfg_input(PIN_EPD_RST, NRF_GPIO_PIN_PULLUP);

	nrf_gpio_pin_clear(PIN_EPD_DC);
	nrf_gpio_cfg_output(PIN_EPD_DC);

	nrf_gpio_cfg_input(PIN_EPD_BUSY, NRF_GPIO_PIN_NOPULL);

	// SPI will be initialized on demand. To prevent spurious driver
	// activation, apply a pullup on the CS pin.
	nrf_gpio_cfg_input(PIN_EPD_CS, NRF_GPIO_PIN_PULLUP);

	// FIXME: remove this.
	// Initial framebuffer pattern for testing.
	for(size_t i = 0; i < sizeof(m_frame_buffer); i++) {
		m_frame_buffer[i] = i % (200 / 8);
	}

	return app_timer_create(&m_sequence_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_sequence_timer);
}


ret_code_t epaper_update(void)
{
	if(m_busy) {
		return NRF_ERROR_BUSY;
	}

	nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
	spi_config.frequency      = NRF_SPIM_FREQ_1M;
	spi_config.ss_pin         = PIN_EPD_CS;
	spi_config.miso_pin       = PIN_EPD_MISO;
	spi_config.mosi_pin       = PIN_EPD_MOSI;
	spi_config.sck_pin        = PIN_EPD_SCK;

	VERIFY_SUCCESS(nrfx_spim_init(&m_spim, &spi_config, cb_spim, NULL));

	// send the power-on sequence after asserting the hardware reset
	m_seq_ptr = FULL_UPDATE_SEQUENCE;
	m_seq_end = FULL_UPDATE_SEQUENCE + (sizeof(FULL_UPDATE_SEQUENCE) / sizeof(FULL_UPDATE_SEQUENCE[0]));

	// assert the hardware reset
	nrf_gpio_pin_clear(PIN_EPD_RST);
	nrf_gpio_cfg_output(PIN_EPD_RST);

	m_timer_state = TIM_RESET;
	VERIFY_SUCCESS(app_timer_start(m_sequence_timer, RESET_ASSERT_TICKS, NULL));

	// the rest will happen asynchronously. See cb_sequence_timer() and cb_spim().
	m_busy = true;

	return NRF_SUCCESS;
}
