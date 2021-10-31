#ifndef EPAPER_H
#define EPAPER_H

#include <sdk_errors.h>

#define EPAPER_COLOR_WHITE    0x01
#define EPAPER_COLOR_BLACK    0x00

#define EPAPER_WIDTH    200
#define EPAPER_HEIGHT   200

/* Structures for Adafruit GFX Font compatibility. */

/// Font data stored PER GLYPH
typedef struct {
	uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
	uint8_t width;         ///< Bitmap dimensions in pixels
	uint8_t height;        ///< Bitmap dimensions in pixels
	uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
	int8_t xOffset;        ///< X dist from cursor pos to UL corner
	int8_t yOffset;        ///< Y dist from cursor pos to UL corner
} GFXglyph;

/// Data stored for FONT AS A WHOLE
typedef struct {
	uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
	GFXglyph *glyph;  ///< Glyph array
	uint16_t first;   ///< ASCII extents (first char)
	uint16_t last;    ///< ASCII extents (last char)
	uint8_t yAdvance; ///< Newline distance (y axis)
} GFXfont;

/* END Adafruid GFX Font compatibility structures. */

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

/**@brief Draw a circle with the given radius around the current cursor location.
 *
 * Use @ref epaper_fb_move_to() to set the center position.
 *
 * The cursor location will not be modified by this function and remain at the
 * center of the circle.
 *
 * @param radius   The radius of the circle in pixels.
 * @param color    The color of the circle.
 */
void epaper_fb_circle(uint8_t radius, uint8_t color);

/**@brief Set the current font.
 *
 * @param font    Pointer to the font to set.
 */
void epaper_fb_set_font(const GFXfont *font);

/**@brief Draw the given character using the current font.
 * @details
 * The character will be drawn at the current cursor position. The cursor will
 * be moved to the right by the width of the character.
 *
 * The font must be previously set using @ref epaper_fb_set_font().
 *
 * @param c       The character to draw (usually an ASCII code).
 * @param color   The color of the text. The background will be unmodified (i.e. transparent).
 *
 * @retval NRF_ERROR_INVALID_STATE    If no font was previously selected.
 * @retval NRF_ERROR_INVALID_PARAM    If the character is not defined for this font.
 * @retval NRF_SUCCESS                If everything was ok.
 */
ret_code_t epaper_fb_draw_char(uint8_t c, uint8_t color);

/**@brief Draw the given string using the current font.
 * @details
 * The string will drawn using repeated calls to @ref epaper_fb_draw_char().
 * Characters missing in the font will be replaced by '?'.
 *
 * Drawing will start at the current cursor position (set it with @ref
 * epaper_fb_move_to() if needed). The cursor will be moved to the right by the
 * width of the drawn string, so the text drawn with multiple calls to this
 * function will be nicely concatenated.
 *
 * @param s      Null-terminated string that should be drawn.
 * @param color  Color of the text. The background will be unchanged (i.e. transparent).
 * @returns      The error code from @ref epaper_fb_draw_char().
 */
ret_code_t epaper_fb_draw_string(char *s, uint8_t color);

#endif // EPAPER_H
