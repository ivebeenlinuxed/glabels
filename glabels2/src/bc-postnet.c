/*
 *  (GLABELS) Label and Business Card Creation program for GNOME
 *
 *  bc_postnet.c:  GLabels POSTNET barcode module
 *
 *  Copyright (C) 2001-2002  Jim Evins <evins@snaught.com>.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/*
 * This module implements the POSTNET barcode specified in the USPS
 * publication 25, Mar 2001.
 */

#include <config.h>

#include <ctype.h>

#include "bc.h"
#include "bc-postnet.h"

#include "debug.h"

static gchar *symbols[] = {
	/* 0 */ "11000",
	/* 1 */ "00011",
	/* 2 */ "00101",
	/* 3 */ "00110",
	/* 4 */ "01001",
	/* 5 */ "01010",
	/* 6 */ "01100",
	/* 7 */ "10001",
	/* 8 */ "10010",
	/* 9 */ "10100",
};

static gchar *frame_symbol = "1";

#define POSTNET_BAR_WIDTH      1.25
#define POSTNET_FULLBAR_HEIGHT 9.00
#define POSTNET_HALFBAR_HEIGHT 3.50
#define POSTNET_BAR_PITCH      3.25
#define POSTNET_HORIZ_MARGIN   9.00
#define POSTNET_VERT_MARGIN    3.00

static gchar *postnet_code (gchar * digits);

/****************************************************************************/
/* Generate list of lines that form the barcode for the given digits.       */
/****************************************************************************/
glBarcode *
gl_barcode_postnet_new (gchar * digits)
{
	gchar *code, *p;
	glBarcode *gbc;
	glBarcodeLine *line;
	gdouble x;

	gbc = g_new0 (glBarcode, 1);

	/* First get code string */
	code = postnet_code (digits);
	if (code == NULL) {
		return NULL;
	}

	/* Now traverse the code string and create a list of lines */
	x = POSTNET_HORIZ_MARGIN;
	for (p = code; *p != 0; p++) {
		line = g_new0 (glBarcodeLine, 1);
		line->x = x;
		line->y = POSTNET_VERT_MARGIN;
		if (*p == '0') {
			line->y +=
			    POSTNET_FULLBAR_HEIGHT - POSTNET_HALFBAR_HEIGHT;
			line->length = POSTNET_HALFBAR_HEIGHT;
		} else {
			line->length = POSTNET_FULLBAR_HEIGHT;
		}
		line->width = POSTNET_BAR_WIDTH;

		gbc->lines = g_list_append (gbc->lines, line);

		x += POSTNET_BAR_PITCH;
	}

	g_free (code);

	gbc->width = x + POSTNET_HORIZ_MARGIN;
	gbc->height = POSTNET_FULLBAR_HEIGHT + 2 * POSTNET_VERT_MARGIN;

	gbc->chars = NULL;

	return gbc;
}

/*--------------------------------------------------------------------------*/
/* PRIVATE.  Generate string of symbols, representing barcode.              */
/*--------------------------------------------------------------------------*/
static gchar *
postnet_code (gchar * digits)
{
	gchar *p;
	gint len;
	gint d, sum;
	GString *code;
	gchar *ret;

	/* Left frame bar */
	code = g_string_new (frame_symbol);

	sum = 0;
	for (p = digits, len = 0; (*p != 0) && (len < 11); p++) {
		if (isdigit (*p)) {
			/* Only translate valid characters (0-9) */
			d = (*p) - '0';
			sum += d;
			code = g_string_append (code, symbols[d]);
			len++;
		}
	}
	if ((len != 5) && (len != 9) && (len != 11)) {
		g_warning ("Invalid POSTNET length %d, should be 5(ZIP), 9(ZIP+4), or 11(DPBC)", len);
	}

	/* Create correction character */
	d = (10 - (sum % 10)) % 10;
	code = g_string_append (code, symbols[d]);

	/* Right frame bar */
	code = g_string_append (code, frame_symbol);

	ret = g_strdup (code->str);
	g_string_free (code, TRUE);

	return ret;
}