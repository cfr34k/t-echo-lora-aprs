#ifndef SDL_DISPLAY_H
#define SDL_DISPLAY_H

#include <stdbool.h>

#include <SDL/SDL.h>

#define EPAPER_COLOR_BLACK 0
#define EPAPER_COLOR_WHITE 1

#define EPAPER_WIDTH  200
#define EPAPER_HEIGHT 200

// drawing lines with this flag set will cause the line to be dashed:
// 5 pixels are drawn, 3 pixels left blank
#define EPAPER_COLOR_FLAG_DASHED 0x02

// drawing lines with this flag set will cause the line to be lightly dotted:
// 1 pixel is drawn, 2 pixels left blank
#define EPAPER_COLOR_FLAG_DOTTED_LIGHT 0x04

#define EPAPER_COLOR_MASK       0x01
#define EPAPER_COLOR_FLAGS_MASK 0xFE

typedef uint32_t ret_code_t;

enum {
	NRF_SUCCESS,
	NRF_ERROR_INVALID_STATE,
	NRF_ERROR_INVALID_PARAM,
};

#define VERIFY_SUCCESS(arg) do { ret_code_t r; if((r = (arg)) != NRF_SUCCESS) { return r; } } while(0);

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

SDL_Surface* init_sdl();

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

/**@brief Draw a rectangle outline.
 *
 * The cursor location will be set to the bottom-left corner.
 *
 * Flipped rectangles (right > left or top > bottom) are not supported and will
 * give undefined results.
 *
 * @param left      X position of the left edge.
 * @param top       Y position of the top edge.
 * @param right     X position of the right edge.
 * @param bottom    Y position of the bottom edge.
 * @param color     The color of the rectangle outline.
 */
void epaper_fb_draw_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color);

/**@brief Draw a filled rectangle.
 *
 * The cursor location will not be modified by this function.
 *
 * Flipped rectangles (right > left or top > bottom) are not supported and will
 * give undefined results.
 *
 * @param left      X position of the left edge.
 * @param top       Y position of the top edge.
 * @param right     X position of the right edge.
 * @param bottom    Y position of the bottom edge.
 * @param color     The color of the rectangle.
 */
void epaper_fb_fill_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color);

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

/**@brief Calculate the rendered width of the given string.
 * @details
 * Calculate the sum of xAdvance values for all printable characters in the
 * string. For non-printable characters, the width of '?' is added, because
 * that is the replacement character used in @ref epaper_fb_draw_string().
 *
 * @param[in] s    The string whose width to calculate.
 * @returns        The number of pixels that the string will occupy in
 *                 horizontal direction.
 */
uint8_t epaper_fb_calc_text_width(const char *s);

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
ret_code_t epaper_fb_draw_string(const char *s, uint8_t color);

/**@brief Draw a binary string with automatic line-wrapping.
 * @details
 * Like @ref epaper_fb_draw_string(), but the cursor will automatically be
 * moved to the next line if a character does not fit onto the screen.
 *
 * @param s      Data array that should be drawn.
 * @param len    Length of the data.
 * @param color  Color of the text. The background will be unchanged (i.e. transparent).
 * @returns      The error code from @ref epaper_fb_draw_char().
 */
ret_code_t epaper_fb_draw_data_wrapped(const uint8_t *s, size_t len, uint8_t color);

/**@brief Wrapper around @ref epaper_fb_draw_data_wrapped() for null-terminated strings.
 * @details
 * This function wraps @ref epaper_fb_draw_data_wrapped() and provides a
 * simplified interface for null-terminated strings.
 *
 * @param s      Null-terminated string that should be drawn.
 * @param color  Color of the text. The background will be unchanged (i.e. transparent).
 * @returns      The error code from @ref epaper_fb_draw_char().
 */
ret_code_t epaper_fb_draw_string_wrapped(const char *s, uint8_t color);

/**@brief Get the line height of the current font.
 *
 * @returns The line height in pixels (zero if no font is set).
 */
uint8_t epaper_fb_get_line_height(void);

/**@brief Get the current cursor x position.
 *
 * @returns The cursor position on the x axis.
 */
uint8_t epaper_fb_get_cursor_pos_x(void);

/**@brief Get the current cursor y position.
 *
 * @returns The cursor position on the y ayis.
 */
uint8_t epaper_fb_get_cursor_pos_y(void);

#endif // SDL_DISPLAY_H
