#ifndef EPAPER_H
#define EPAPER_H

#include <sdk_errors.h>

/**@brief Initialize the ePaper driver.
 * @details
 * This only initializes the GPIOs and sets them to a safe state. SPI is
 * initialized and shut down on demand, i.e. only while a control sequence is
 * executed.
 *
 * @returns    A result code from initializing the internal app_timer.
 */
ret_code_t epaper_init(void);

/**@brief Update the display with the current framebuffer.
 * @details
 * This function starts a full update sequence with the current framebuffer. It
 * will reset the display (to recover from deep sleep), load the LUT, send the
 * data, wait for completion, and put the display to deep sleep again.
 *
 * The whole sequence will be executed asynchronously and this function will
 * return NRF_ERROR_BUSY until the sequence is complete.
 *
 * @returns    A result code from starting the sequence.
 */
ret_code_t epaper_update(void);

#endif // EPAPER_H
