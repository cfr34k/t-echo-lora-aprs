#include "sdl_display.h"
#include "SDL_video.h"

#include "fasttrigon.h"

typedef struct
{
	uint8_t x;
	uint8_t y;
} point_t;

SDL_Surface *screen;

static uint32_t white;
static uint32_t black;

static point_t m_cursor;
static const GFXfont *m_font;

SDL_Surface* init_sdl(int w, int h)
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		return NULL;
	}

	if((screen = SDL_SetVideoMode(EPAPER_WIDTH, EPAPER_HEIGHT, 24, SDL_HWSURFACE | SDL_DOUBLEBUF)) == NULL) {
		printf("Could not set SDL video mode: %s\n", SDL_GetError());
		return NULL;
	}

	white = SDL_MapRGB(screen->format, 255, 255, 255);
	black = SDL_MapRGB(screen->format, 0, 0, 0);

	SDL_Rect rect = {0, 0, EPAPER_WIDTH, EPAPER_HEIGHT};
	SDL_FillRect(screen, &rect, white);

	SDL_WM_SetCaption("Menusystem Test", "Menusystem Test");

	return screen;
}

// from the SDL docs
void epaper_fb_set_pixel(uint8_t x, uint8_t y, uint8_t color)
{
	if(x >= EPAPER_WIDTH || y >= EPAPER_HEIGHT) {
		return;
	}

	int bpp = screen->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to set */
	Uint8 *p = (Uint8 *)screen->pixels + y * screen->pitch + x * bpp;

	uint32_t pixel;
	switch(color & EPAPER_COLOR_MASK)
	{
		case EPAPER_COLOR_WHITE: pixel = white; break;
		default:                 pixel = black; break;
	}

	switch(bpp) {
		case 1:
			*p = pixel;
			break;

		case 2:
			*(Uint16 *)p = pixel;
			break;

		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
			} else {
				p[0] = pixel & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = (pixel >> 16) & 0xff;
			}
			break;

		case 4:
			*(Uint32 *)p = pixel;
			break;
	}
}

void epaper_fb_clear(uint8_t color)
{
	uint32_t c;
	if(color) {
		c = white;
	} else {
		c = black;
	}

	SDL_Rect rect = {0, 0, EPAPER_WIDTH, EPAPER_HEIGHT};
	SDL_FillRect(screen, &rect, c);
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

	bool is_dashed = (color & EPAPER_COLOR_FLAG_DASHED) != 0;
	bool is_dotted = (color & EPAPER_COLOR_FLAG_DOTTED_LIGHT) != 0;

	while(x <= dx) {
		int16_t tx = neg_x ? -x : x;
		int16_t ty = neg_y ? -y : y;

		bool draw_pixel = true;

		if(is_dashed) {
			draw_pixel = (pixcount % 8) < 5;
		} else if(is_dotted) {
			draw_pixel = (pixcount % 3) == 0;
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
