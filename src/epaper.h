#ifndef EPAPER_H
#define EPAPER_H

#include <sdk_errors.h>

#define EPAPER_COLOR_WHITE    0x01
#define EPAPER_COLOR_BLACK    0x00

#define EPAPER_WIDTH    200
#define EPAPER_HEIGHT   200

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

/**@brief Clear the frame buffer with the specified color.
 *
 * @param color   Either EPAPER_COLOR_BLACK or EPAPER_COLOR_WHITE.
 */
void epaper_fb_clear(uint8_t color);

/**@brief Set the specified pixel to the given color.
 *
 * @param x       The x coordinate of the pixel.
 * @param y       The y coordinate of the pixel.
 * @param color   Either EPAPER_COLOR_BLACK or EPAPER_COLOR_WHITE.
 */
void epaper_fb_set_pixel(uint8_t x, uint8_t y, uint8_t color);

/**@brief Set the location of the line-drawing cursor.
 *
 * @param x       The cursor's new x coordinate.
 * @param y       The cursor's new y coordinate.
 */
void epaper_fb_move_to(uint8_t x, uint8_t y);

/**@brief Draw a line from the cursor to the given location.
 * @details
 * The cursor will be updated to the line's end coordinates.
 *
 * @param xe      Target x coordinate.
 * @param ye      Target y coordinate.
 * @param color   The color of the line.
 */
void epaper_fb_line_to(uint8_t xe, uint8_t ye, uint8_t color);

#endif // EPAPER_H
