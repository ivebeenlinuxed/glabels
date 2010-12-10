/*
 *  merge-text.c
 *  Copyright (C) 2001-2009  Jim Evins <evins@snaught.com>.
 *
 *  This file is part of gLabels.
 *
 *  gLabels is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  gLabels is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with gLabels.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "merge-text.h"

#include <stdio.h>

#include "debug.h"

#define LINE_BUF_LEN 1024


/*===========================================*/
/* Private types                             */
/*===========================================*/

struct _glMergeTextPrivate {

	gchar             delim;
        gboolean          line1_has_keys;

	FILE             *fp;

        GPtrArray        *keys;
        gint              n_fields_max;
};

enum {
	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_DELIM,
	ARG_LINE1_HAS_KEYS,
};


/*===========================================*/
/* Private globals                           */
/*===========================================*/


/*===========================================*/
/* Local function prototypes                 */
/*===========================================*/

static void           gl_merge_text_finalize        (GObject          *object);

static void           gl_merge_text_set_property    (GObject          *object,
						     guint             param_id,
						     const GValue     *value,
						     GParamSpec       *pspec);

static void           gl_merge_text_get_property    (GObject          *object,
						     guint             param_id,
						     GValue           *value,
						     GParamSpec       *pspec);

static gchar         *key_from_index                (glMergeText      *merge_text,
                                                     gint              i_field);
static void           clear_keys                    (glMergeText      *merge_text);

static GList         *gl_merge_text_get_key_list    (glMerge          *merge);
static gchar         *gl_merge_text_get_primary_key (glMerge          *merge);
static void           gl_merge_text_open            (glMerge          *merge);
static void           gl_merge_text_close           (glMerge          *merge);
static glMergeRecord *gl_merge_text_get_record      (glMerge          *merge);
static void           gl_merge_text_copy            (glMerge          *dst_merge,
						     glMerge          *src_merge);

static GList         *parse_line                    (FILE             *fp,
						     gchar             delim);
static gchar         *parse_field                   (gchar            *raw_field);
static void           free_fields                   (GList           **fields);



/*****************************************************************************/
/* Boilerplate object stuff.                                                 */
/*****************************************************************************/
G_DEFINE_TYPE (glMergeText, gl_merge_text, GL_TYPE_MERGE);


static void
gl_merge_text_class_init (glMergeTextClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	glMergeClass *merge_class  = GL_MERGE_CLASS (class);

	gl_debug (DEBUG_MERGE, "START");

	gl_merge_text_parent_class = g_type_class_peek_parent (class);

	object_class->set_property = gl_merge_text_set_property;
	object_class->get_property = gl_merge_text_get_property;

	g_object_class_install_property
                (object_class,
                 ARG_DELIM,
                 g_param_spec_char ("delim", NULL, NULL,
				    0, 0x7F, ',',
				    (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property
                (object_class,
                 ARG_LINE1_HAS_KEYS,
                 g_param_spec_boolean ("line1_has_keys", NULL, NULL,
                                       FALSE,
                                       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->finalize = gl_merge_text_finalize;

	merge_class->get_key_list    = gl_merge_text_get_key_list;
	merge_class->get_primary_key = gl_merge_text_get_primary_key;
	merge_class->open            = gl_merge_text_open;
	merge_class->close           = gl_merge_text_close;
	merge_class->get_record      = gl_merge_text_get_record;
	merge_class->copy            = gl_merge_text_copy;

	gl_debug (DEBUG_MERGE, "END");
}


static void
gl_merge_text_init (glMergeText *merge_text)
{
	gl_debug (DEBUG_MERGE, "START");

	merge_text->priv = g_new0 (glMergeTextPrivate, 1);

        merge_text->priv->keys = g_ptr_array_new ();

	gl_debug (DEBUG_MERGE, "END");
}


static void
gl_merge_text_finalize (GObject *object)
{
	glMergeText *merge_text = GL_MERGE_TEXT (object);

	gl_debug (DEBUG_MERGE, "START");

	g_return_if_fail (object && GL_IS_MERGE_TEXT (object));

        clear_keys (merge_text);
        g_ptr_array_free (merge_text->priv->keys, TRUE);
	g_free (merge_text->priv);

	G_OBJECT_CLASS (gl_merge_text_parent_class)->finalize (object);

	gl_debug (DEBUG_MERGE, "END");
}


/*--------------------------------------------------------------------------*/
/* Set argument.                                                            */
/*--------------------------------------------------------------------------*/
static void
gl_merge_text_set_property (GObject      *object,
			    guint         param_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	glMergeText *merge_text;

	merge_text = GL_MERGE_TEXT (object);

	switch (param_id) {

	case ARG_DELIM:
		merge_text->priv->delim = g_value_get_char (value);
		gl_debug (DEBUG_MERGE, "ARG \"delim\" = \"%c\"",
			  merge_text->priv->delim);
		break;

        case ARG_LINE1_HAS_KEYS:
                merge_text->priv->line1_has_keys = g_value_get_boolean (value);
		gl_debug (DEBUG_MERGE, "ARG \"line1_has_keys\" = \"%d\"",
			  merge_text->priv->line1_has_keys);
		break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;

        }

}


/*--------------------------------------------------------------------------*/
/* Get argument.                                                            */
/*--------------------------------------------------------------------------*/
static void
gl_merge_text_get_property (GObject     *object,
			    guint        param_id,
			    GValue      *value,
			    GParamSpec  *pspec)
{
	glMergeText *merge_text;

	merge_text = GL_MERGE_TEXT (object);

	switch (param_id) {

	case ARG_DELIM:
		g_value_set_char (value, merge_text->priv->delim);
		break;

        case ARG_LINE1_HAS_KEYS:
                g_value_set_boolean (value, merge_text->priv->line1_has_keys);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;

        }

}


/*---------------------------------------------------------------------------*/
/* Lookup key name from zero based index.                                    */
/*---------------------------------------------------------------------------*/
static gchar *
key_from_index (glMergeText  *merge_text,
                gint          i_field)
{
        if ( merge_text->priv->line1_has_keys &&
             (i_field < merge_text->priv->keys->len) )
        {
                return g_strdup (g_ptr_array_index (merge_text->priv->keys, i_field));
        }
        else
        {
                return g_strdup_printf ("%d", i_field+1);
        }
}


/*---------------------------------------------------------------------------*/
/* Clear stored keys.                                                        */
/*---------------------------------------------------------------------------*/
static void
clear_keys (glMergeText      *merge_text)
{
        gint i;

        for ( i = 0; i < merge_text->priv->keys->len; i++ )
        {
                g_free (g_ptr_array_index (merge_text->priv->keys, i));
        }
        merge_text->priv->keys->len = 0;
}


/*--------------------------------------------------------------------------*/
/* Get key list.                                                            */
/*--------------------------------------------------------------------------*/
static GList *
gl_merge_text_get_key_list (glMerge *merge)
{
	glMergeText   *merge_text;
	gint           i_field, n_fields;
	GList         *key_list;
	
	gl_debug (DEBUG_MERGE, "BEGIN");

	merge_text = GL_MERGE_TEXT (merge);

        if ( merge_text->priv->line1_has_keys )
        {
                n_fields = merge_text->priv->keys->len;
        }
        else
        {
                n_fields = merge_text->priv->n_fields_max;
        }

        key_list = NULL;
        for ( i_field=0; i_field < n_fields; i_field++ )
        {
                key_list = g_list_append (key_list, key_from_index(merge_text, i_field));
        }

	gl_debug (DEBUG_MERGE, "END");

	return key_list;
}


/*--------------------------------------------------------------------------*/
/* Get "primary" key.                                                       */
/*--------------------------------------------------------------------------*/
static gchar *
gl_merge_text_get_primary_key (glMerge *merge)
{
	/* For now, let's always assume the first column is the primary key. */
        return key_from_index (GL_MERGE_TEXT (merge), 0);
}


/*--------------------------------------------------------------------------*/
/* Open merge source.                                                       */
/*--------------------------------------------------------------------------*/
static void
gl_merge_text_open (glMerge *merge)
{
	glMergeText *merge_text;
	gchar       *src;

        GList       *line1_fields;
        GList       *p;

	merge_text = GL_MERGE_TEXT (merge);

	src = gl_merge_get_src (merge);

	if (src != NULL)
        {
		if (g_utf8_strlen(src, -1) == 1 && src[0] == '-')
			merge_text->priv->fp = stdin;
		else
			merge_text->priv->fp = fopen (src, "r");

                g_free (src);

                clear_keys (merge_text);
                merge_text->priv->n_fields_max = 0;

                if ( merge_text->priv->line1_has_keys )
                {
                        /*
                         * Extract keys from first line and discard line
                         */

                        line1_fields = parse_line (merge_text->priv->fp, merge_text->priv->delim);
                        for ( p = line1_fields; p != NULL; p = p->next )
                        {
                                g_ptr_array_add (merge_text->priv->keys, g_strdup (p->data));
                        }
                        free_fields (&line1_fields);
                }

	}


}


/*--------------------------------------------------------------------------*/
/* Close merge source.                                                      */
/*--------------------------------------------------------------------------*/
static void
gl_merge_text_close (glMerge *merge)
{
	glMergeText *merge_text;

	merge_text = GL_MERGE_TEXT (merge);

	if (merge_text->priv->fp != NULL) {

		fclose (merge_text->priv->fp);
		merge_text->priv->fp = NULL;

	}
}


/*--------------------------------------------------------------------------*/
/* Get next record from merge source, NULL if no records left (i.e EOF)     */
/*--------------------------------------------------------------------------*/
static glMergeRecord *
gl_merge_text_get_record (glMerge *merge)
{
	glMergeText   *merge_text;
	gchar          delim;
	FILE          *fp;
	glMergeRecord *record;
	GList         *fields, *p;
	gint           i_field;
	glMergeField  *field;

	merge_text = GL_MERGE_TEXT (merge);

	delim = merge_text->priv->delim;
	fp    = merge_text->priv->fp;

	fields = parse_line (fp, delim);
	if ( fields == NULL ) {
		return NULL;
	}

	record = g_new0 (glMergeRecord, 1);
	record->select_flag = TRUE;
	for (p=fields, i_field=0; p != NULL; p=p->next, i_field++) {

		field = g_new0 (glMergeField, 1);
                field->key = key_from_index (merge_text, i_field);
#ifdef CSV_NOT_ALWAYS_UTF8
		field->value = g_locale_to_utf8 (p->data, -1, NULL, NULL, NULL);
#else
		field->value = g_strdup (p->data);
#endif

		record->field_list = g_list_append (record->field_list, field);
	}
	free_fields (&fields);

        if ( i_field > merge_text->priv->n_fields_max )
        {
                merge_text->priv->n_fields_max = i_field;
        }

	return record;
}


/*---------------------------------------------------------------------------*/
/* Copy merge_text specific fields.                                          */
/*---------------------------------------------------------------------------*/
static void
gl_merge_text_copy (glMerge *dst_merge,
		    glMerge *src_merge)
{
	glMergeText *dst_merge_text;
	glMergeText *src_merge_text;
        gint         i;

	dst_merge_text = GL_MERGE_TEXT (dst_merge);
	src_merge_text = GL_MERGE_TEXT (src_merge);

	dst_merge_text->priv->delim          = src_merge_text->priv->delim;
	dst_merge_text->priv->line1_has_keys = src_merge_text->priv->line1_has_keys;

        for ( i=0; i < src_merge_text->priv->keys->len; i++ )
        {
                g_ptr_array_add (dst_merge_text->priv->keys,
                                 g_strdup ((gchar *)g_ptr_array_index (src_merge_text->priv->keys, i)));
        }

	dst_merge_text->priv->n_fields_max   = src_merge_text->priv->n_fields_max;
}


/*---------------------------------------------------------------------------*/
/* PRIVATE.  Parse line.                                                     */
/*                                                                           */
/* Attempt to be a robust parser of various CSV (and similar) formats.       */
/*                                                                           */
/* Split into fields, accounting for:                                        */
/*   - delimeters may be embedded in quoted text (")                         */
/*   - delimeters may be "escaped" by a leading backslash (\)                */
/*   - quotes may be embedded in quoted text as two adjacent quotes ("")     */
/*   - quotes may be "escaped" either within or outside of quoted text.      */
/*   - newlines may be embedded in quoted text, allowing a field to span     */
/*     more than one line.                                                   */
/*                                                                           */
/* This function does not do any parsing of the individual fields, other     */
/* than to correctly interpet delimeters.  Actual parsing of the individual  */
/* fields is done in parse_field().                                          */
/*                                                                           */
/* Returns a list of fields.  A blank line is considered a line with one     */
/* empty field.  Returns empty (NULL) when done.                             */
/*---------------------------------------------------------------------------*/
static GList *
parse_line (FILE  *fp,
	    gchar  delim )
{
	GList *list = NULL;
	GString *string;
	gint c;
	enum { BEGIN, NORMAL, QUOTED, QUOTED_QUOTE1,
               NORMAL_ESCAPED, QUOTED_ESCAPED, DONE } state;

	if (fp == NULL) {
		return NULL;
	}
	       
	state = BEGIN;
	string = g_string_new( "" );
	while ( state != DONE ) {
		c=getc (fp);

		switch (state) {

		case BEGIN:
                        if ( c == delim )
                        {
                                /* first field is empty. */
                                list = g_list_append (list, g_strdup (""));
				state = NORMAL;
                                break;
                        }
			switch (c) {
			case '"':
                                string = g_string_append_c (string, c);
				state = QUOTED;
				break;
			case '\\':
                                string = g_string_append_c (string, c);
				state = NORMAL_ESCAPED;
				break;
			case '\n':
				/* treat as one empty field. */
				list = g_list_append (list, g_strdup (""));
				state = DONE;
				break;
			case EOF:
                                /* end of file, no more lines. */
				state = DONE;
				break;
			default:
                                string = g_string_append_c (string, c);
				state = NORMAL;
				break;
			}
			break;

		case NORMAL:
                        if ( c == delim )
                        {
                                list = g_list_append (list, parse_field (string->str));
                                string = g_string_assign( string, "" );
                                state = NORMAL;
                                break;
                        }
			switch (c) {
			case '"':
                                string = g_string_append_c (string, c);
				state = QUOTED;
				break;
			case '\\':
                                string = g_string_append_c (string, c);
				state = NORMAL_ESCAPED;
				break;
			case '\n':
			case EOF:
				list = g_list_append (list, parse_field (string->str));
				state = DONE;
				break;
			default:
                                string = g_string_append_c (string, c);
                                state = NORMAL;
				break;
			}
			break;

		case QUOTED:
			switch (c) {
			case '"':
                                string = g_string_append_c (string, c);
				state = QUOTED_QUOTE1;
				break;
			case '\\':
                                string = g_string_append_c (string, c);
				state = QUOTED_ESCAPED;
				break;
			case EOF:
				/* File ended mid way through quoted item */
				list = g_list_append (list, parse_field (string->str));
				state = DONE;
				break;
			default:
				string = g_string_append_c (string, c);
				break;
			}
			break;

		case QUOTED_QUOTE1:
                        if ( c == delim )
                        {
                                list = g_list_append (list, parse_field (string->str));
                                string = g_string_assign( string, "" );
                                state = NORMAL;
                                break;
                        }
			switch (c) {
			case '"':
				/* insert quotes in string, stay quoted. */
				string = g_string_append_c (string, c);
				state = QUOTED;
				break;
			case '\n':
			case EOF:
				/* line or file ended after quoted item */
				list = g_list_append (list, parse_field (string->str));
				state = DONE;
				break;
			default:
                                string = g_string_append_c (string, c);
				state = NORMAL;
				break;
			}
			break;

		case NORMAL_ESCAPED:
			switch (c) {
			case EOF:
				/* File ended mid way through quoted item */
				list = g_list_append (list, parse_field (string->str));
				state = DONE;
				break;
			default:
				string = g_string_append_c (string, c);
				state = NORMAL;
				break;
			}
			break;

		case QUOTED_ESCAPED:
			switch (c) {
			case EOF:
				/* File ended mid way through quoted item */
				list = g_list_append (list, parse_field (string->str));
				state = DONE;
				break;
			default:
				string = g_string_append_c (string, c);
				state = QUOTED;
				break;
			}
			break;

		default:
			g_assert_not_reached();
			break;
		}

	}
	g_string_free( string, TRUE );

	return list;
}


/*---------------------------------------------------------------------------*/
/* PRIVATE.  Parse field.                                                    */
/*                                                                           */
/*  - Strip leading and trailing white space, unless quoted.                 */
/*  - Strip CR, unless escaped.                                              */
/*  - Expand '\n' and '\t' into newline and tab characters.                  */
/*  - Remove quotes, unless escaped (\" anywhere or "" within quotes)        */
/*---------------------------------------------------------------------------*/
static gchar *
parse_field (gchar  *raw_field)
{
	GString *string;
        gchar   *pass1_field, *c, *field;
	enum { NORMAL, NORMAL_ESCAPED, QUOTED, QUOTED_ESCAPED, QUOTED_QUOTE1} state;


        /*
         * Pass 1: remove leading and trailing spaces.
         */
        pass1_field = g_strdup (raw_field);
        g_strstrip (pass1_field);

        /*
         * Pass 2: resolve quoting and escaping.
         */
	state = NORMAL;
	string = g_string_new( "" );
        for ( c=pass1_field; *c != 0; c++ )
        {
		switch (state) {

		case NORMAL:
			switch (*c) {
			case '\\':
				state = NORMAL_ESCAPED;
				break;
			case '"':
				state = QUOTED;
				break;
			case '\r':
				/* Strip CR. */
				break;
			default:
                                string = g_string_append_c (string, *c);
				break;
			}
			break;

		case NORMAL_ESCAPED:
			switch (*c) {
			case 'n':
				string = g_string_append_c (string, '\n');
				state = NORMAL;
				break;
			case 't':
				string = g_string_append_c (string, '\t');
				state = NORMAL;
				break;
			default:
				string = g_string_append_c (string, *c);
				state = NORMAL;
				break;
			}
			break;

		case QUOTED:
			switch (*c) {
			case '\\':
				state = QUOTED_ESCAPED;
				break;
			case '"':
				state = QUOTED_QUOTE1;
				break;
			case '\r':
				/* Strip CR. */
				break;
			default:
				string = g_string_append_c (string, *c);
				break;
			}
			break;

		case QUOTED_ESCAPED:
			switch (*c) {
			case 'n':
				string = g_string_append_c (string, '\n');
				state = QUOTED;
				break;
			case 't':
				string = g_string_append_c (string, '\t');
				state = QUOTED;
				break;
			default:
				string = g_string_append_c (string, *c);
				state = QUOTED;
				break;
			}
			break;

		case QUOTED_QUOTE1:
			switch (*c) {
			case '"':
				/* insert quotes in string, stay quoted. */
				string = g_string_append_c (string, *c);
				state = QUOTED;
				break;
			case '\r':
				/* Strip CR, return to QUOTED. */
				state = QUOTED;
				break;
			default:
                                string = g_string_append_c (string, *c);
				state = NORMAL;
				break;
			}
			break;

		default:
			g_assert_not_reached();
			break;
		}

	}

        field = g_strdup (string->str);
	g_string_free( string, TRUE );
        g_free (pass1_field);

	return field;
}


/*---------------------------------------------------------------------------*/
/* Free list of fields.                                                      */
/*---------------------------------------------------------------------------*/
void
free_fields (GList ** list)
{
	GList *p;

	for (p = *list; p != NULL; p = p->next) {
		g_free (p->data);
		p->data = NULL;
	}

	g_list_free (*list);
	*list = NULL;
}



/*
 * Local Variables:       -- emacs
 * mode: C                -- emacs
 * c-basic-offset: 8      -- emacs
 * tab-width: 8           -- emacs
 * indent-tabs-mode: nil  -- emacs
 * End:                   -- emacs
 */
