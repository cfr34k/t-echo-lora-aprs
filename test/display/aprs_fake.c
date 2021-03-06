#include <string.h>

#include "aprs.h"

static char m_table;
static char m_icon;

static char m_dest[32];
static char m_src[32];

void aprs_get_icon(char *table, char *icon)
{
	*table = m_table;
	*icon = m_icon;
}

void aprs_set_icon(char table, char icon)
{
	m_table = table;
	m_icon  = icon;
}

const char* aprs_get_parser_error(void)
{
	return "Something went seriosly wrong. Seriously.";
}

bool aprs_can_build_frame(void)
{
	return true;
}


void aprs_set_dest(const char *dest)
{
	strncpy(m_dest, dest, sizeof(m_dest));
}

void aprs_get_dest(char *dest, size_t dest_len)
{
	strncpy(dest, m_dest, dest_len);
}

void aprs_set_source(const char *call)
{
	strncpy(m_src, call, sizeof(m_src));
}

void aprs_get_source(char *source, size_t source_len)
{
	strncpy(source, m_src, source_len);
}
