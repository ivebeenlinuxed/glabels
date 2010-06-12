/*
 *  label-image.c
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

#include "label-image.h"

#include <glib/gi18n.h>
#include <glib.h>
#include <gdk/gdk.h>

#include "pixmaps/checkerboard.xpm"

#include "debug.h"


#define MIN_IMAGE_SIZE 1.0


/*========================================================*/
/* Private types.                                         */
/*========================================================*/

struct _glLabelImagePrivate {
	glTextNode       *filename;
	GdkPixbuf        *pixbuf;
};


/*========================================================*/
/* Private globals.                                       */
/*========================================================*/

static GdkPixbuf *default_pixbuf = NULL;


/*========================================================*/
/* Private function prototypes.                           */
/*========================================================*/

static void gl_label_image_finalize      (GObject           *object);

static void copy                         (glLabelObject     *dst_object,
					  glLabelObject     *src_object);

static void set_size                     (glLabelObject     *object,
                                          gdouble            w,
                                          gdouble            h,
                                          gboolean           checkpoint);

static void draw_object                  (glLabelObject     *object,
                                          cairo_t           *cr,
                                          gboolean           screen_flag,
                                          glMergeRecord     *record);

static gboolean object_at                (glLabelObject     *object,
                                          cairo_t           *cr,
                                          gdouble            x_pixels,
                                          gdouble            y_pixels);


/*****************************************************************************/
/* Boilerplate object stuff.                                                 */
/*****************************************************************************/
G_DEFINE_TYPE (glLabelImage, gl_label_image, GL_TYPE_LABEL_OBJECT);


static void
gl_label_image_class_init (glLabelImageClass *class)
{
	GObjectClass       *object_class       = G_OBJECT_CLASS (class);
	glLabelObjectClass *label_object_class = GL_LABEL_OBJECT_CLASS (class);

	gl_label_image_parent_class = g_type_class_peek_parent (class);

	label_object_class->copy              = copy;
	label_object_class->set_size          = set_size;
        label_object_class->draw_object       = draw_object;
        label_object_class->draw_shadow       = NULL;
        label_object_class->object_at         = object_at;

	object_class->finalize = gl_label_image_finalize;
}


static void
gl_label_image_init (glLabelImage *limage)
{
        GdkPixbuf *pixbuf;

	if ( default_pixbuf == NULL ) {
		pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **)checkerboard_xpm);
                default_pixbuf =
                        gdk_pixbuf_scale_simple (pixbuf, 128, 128, GDK_INTERP_NEAREST);
                g_object_unref (pixbuf);
	}

	limage->priv = g_new0 (glLabelImagePrivate, 1);

	limage->priv->filename = g_new0 (glTextNode, 1);

	limage->priv->pixbuf = default_pixbuf;
}


static void
gl_label_image_finalize (GObject *object)
{
	glLabelObject *lobject = GL_LABEL_OBJECT (object);
	glLabelImage  *limage  = GL_LABEL_IMAGE (object);
        glLabel       *label;
	GHashTable    *pixbuf_cache;

	g_return_if_fail (object && GL_IS_LABEL_IMAGE (object));

	if (!limage->priv->filename->field_flag) {
                label = gl_label_object_get_parent (lobject);
		pixbuf_cache = gl_label_get_pixbuf_cache (label);
		gl_pixbuf_cache_remove_pixbuf (pixbuf_cache,
					       limage->priv->filename->data);
	}
	gl_text_node_free (&limage->priv->filename);
	g_free (limage->priv);

	G_OBJECT_CLASS (gl_label_image_parent_class)->finalize (object);
}


/*****************************************************************************/
/* NEW label "image" object.                                                 */
/*****************************************************************************/
GObject *
gl_label_image_new (glLabel *label,
                    gboolean checkpoint)
{
	glLabelImage *limage;

	limage = g_object_new (gl_label_image_get_type(), NULL);

        if (label != NULL)
        {
                if ( checkpoint )
                {
                        gl_label_checkpoint (label, _("Create image object"));
                }

                gl_label_add_object (label, GL_LABEL_OBJECT (limage));
                gl_label_object_set_parent (GL_LABEL_OBJECT (limage), label);
        }

	return G_OBJECT (limage);
}


/*****************************************************************************/
/* Copy object contents.                                                     */
/*****************************************************************************/
static void
copy (glLabelObject *dst_object,
      glLabelObject *src_object)
{
	glLabelImage     *limage     = (glLabelImage *)src_object;
	glLabelImage     *new_limage = (glLabelImage *)dst_object;
	glTextNode       *filename;
	GdkPixbuf        *pixbuf;
        glLabel          *label;
	GHashTable       *pixbuf_cache;

	gl_debug (DEBUG_LABEL, "START");

	g_return_if_fail (limage && GL_IS_LABEL_IMAGE (limage));
	g_return_if_fail (new_limage && GL_IS_LABEL_IMAGE (new_limage));

	filename = gl_label_image_get_filename (limage);

	/* Make sure destination label has data suitably cached. */
	if ( !filename->field_flag && (filename->data != NULL) ) {
		pixbuf = limage->priv->pixbuf;
		if ( pixbuf != default_pixbuf ) {
                        label = gl_label_object_get_parent (dst_object);
			pixbuf_cache = gl_label_get_pixbuf_cache (label);
			gl_pixbuf_cache_add_pixbuf (pixbuf_cache, filename->data, pixbuf);
		}
	}

	gl_label_image_set_filename (new_limage, filename, FALSE);
	gl_text_node_free (&filename);

	gl_debug (DEBUG_LABEL, "END");
}


/*---------------------------------------------------------------------------*/
/* PRIVATE.  Set size method.                                                */
/*---------------------------------------------------------------------------*/
static void
set_size (glLabelObject *object,
	  gdouble        w,
	  gdouble        h,
          gboolean       checkpoint)
{
	g_return_if_fail (object && GL_IS_LABEL_OBJECT (object));

        if (w < MIN_IMAGE_SIZE)
        {
                w = MIN_IMAGE_SIZE;
        }

        if (h < MIN_IMAGE_SIZE)
        {
                h = MIN_IMAGE_SIZE;
        }

	GL_LABEL_OBJECT_CLASS (gl_label_image_parent_class)->set_size (object, w, h, checkpoint);
}


/*****************************************************************************/
/* Set object params.                                                        */
/*****************************************************************************/
void
gl_label_image_set_filename (glLabelImage *limage,
			     glTextNode   *filename,
                             gboolean      checkpoint)
{
	glTextNode  *old_filename;
        glLabel     *label;
	GHashTable  *pixbuf_cache;
	GdkPixbuf   *pixbuf;
	gdouble      image_w, image_h, aspect_ratio, w, h;

	gl_debug (DEBUG_LABEL, "START");

	g_return_if_fail (limage && GL_IS_LABEL_IMAGE (limage));
	g_return_if_fail (filename != NULL);

	old_filename = limage->priv->filename;

	/* If Unchanged don't do anything */
	if ( gl_text_node_equal (filename, old_filename ) ) {
		return;
	}

        label = gl_label_object_get_parent (GL_LABEL_OBJECT (limage));

        if ( checkpoint )
        {
                gl_label_checkpoint (label, _("Set image"));
        }

	pixbuf_cache = gl_label_get_pixbuf_cache (label);

	/* Remove reference to previous pixbuf from cache, if needed. */
	if ( !old_filename->field_flag && (old_filename->data != NULL) ) {
		gl_pixbuf_cache_remove_pixbuf (pixbuf_cache, old_filename->data);
	}

	/* Set new filename. */
	limage->priv->filename = gl_text_node_dup(filename);
	gl_text_node_free (&old_filename);

	/* Now set the pixbuf. */
	if ( filename->field_flag || (filename->data == NULL) ) {

		limage->priv->pixbuf = default_pixbuf;

	} else {

		pixbuf = gl_pixbuf_cache_get_pixbuf (pixbuf_cache, filename->data);

		if (pixbuf != NULL) {
			limage->priv->pixbuf = pixbuf;
		} else {
			limage->priv->pixbuf = default_pixbuf;
		}
	}

	/* Treat current size as a bounding box, scale image to maintain aspect
	 * ratio while fitting it in this bounding box. */
	image_w = gdk_pixbuf_get_width (limage->priv->pixbuf);
	image_h = gdk_pixbuf_get_height (limage->priv->pixbuf);
	aspect_ratio = image_h / image_w;
	gl_label_object_get_size (GL_LABEL_OBJECT(limage), &w, &h);
	if ( h > w*aspect_ratio ) {
		h = w * aspect_ratio;
	} else {
		w = h / aspect_ratio;
	}
	gl_label_object_set_size (GL_LABEL_OBJECT(limage), w, h, FALSE);

	gl_label_object_emit_changed (GL_LABEL_OBJECT(limage));

	gl_debug (DEBUG_LABEL, "END");
}


void
gl_label_image_set_pixbuf (glLabelImage  *limage,
                           GdkPixbuf     *pixbuf,
                           gboolean       checkpoint)
{
	glTextNode  *old_filename;
        glLabel     *label;
	GHashTable  *pixbuf_cache;
        gchar       *name;
	gdouble      image_w, image_h;

	gl_debug (DEBUG_LABEL, "START");

	g_return_if_fail (limage && GL_IS_LABEL_IMAGE (limage));
	g_return_if_fail (pixbuf && GDK_IS_PIXBUF (pixbuf));

	old_filename = limage->priv->filename;

        label = gl_label_object_get_parent (GL_LABEL_OBJECT (limage));

        if ( checkpoint )
        {
                gl_label_checkpoint (label, _("Set image"));
        }

	pixbuf_cache = gl_label_get_pixbuf_cache (label);

	/* Remove reference to previous pixbuf from cache, if needed. */
	if ( !old_filename->field_flag && (old_filename->data != NULL) ) {
		gl_pixbuf_cache_remove_pixbuf (pixbuf_cache, old_filename->data);
	}

	/* Set new filename. */
        name = g_compute_checksum_for_data (G_CHECKSUM_MD5,
                                            gdk_pixbuf_get_pixels (pixbuf),
                                            gdk_pixbuf_get_rowstride (pixbuf)*gdk_pixbuf_get_height (pixbuf));
	limage->priv->filename = gl_text_node_new_from_text(name);
	gl_text_node_free (&old_filename);

        limage->priv->pixbuf = g_object_ref (pixbuf);
        gl_pixbuf_cache_add_pixbuf (pixbuf_cache, name, pixbuf);

        g_free (name);

	image_w = gdk_pixbuf_get_width (limage->priv->pixbuf);
	image_h = gdk_pixbuf_get_height (limage->priv->pixbuf);
	gl_label_object_set_size (GL_LABEL_OBJECT(limage), image_w, image_h, FALSE);

	gl_label_object_emit_changed (GL_LABEL_OBJECT(limage));

	gl_debug (DEBUG_LABEL, "END");
}


/*****************************************************************************/
/* Get object params.                                                        */
/*****************************************************************************/
glTextNode *
gl_label_image_get_filename (glLabelImage *limage)
{
	g_return_val_if_fail (limage && GL_IS_LABEL_IMAGE (limage), NULL);

	return gl_text_node_dup (limage->priv->filename);
}


const GdkPixbuf *
gl_label_image_get_pixbuf (glLabelImage  *limage,
			   glMergeRecord *record)
{
	g_return_val_if_fail (limage && GL_IS_LABEL_IMAGE (limage), NULL);

	if ((record != NULL) && limage->priv->filename->field_flag) {

		GdkPixbuf   *pixbuf = NULL;
		gchar       *real_filename;

		/* Indirect filename, re-evaluate for given record. */

		real_filename = gl_merge_eval_key (record,
						   limage->priv->filename->data);

		if (real_filename != NULL) {
			pixbuf = gdk_pixbuf_new_from_file (real_filename, NULL);
		}
		if ( pixbuf != NULL ) {
			return pixbuf;
		} else {
			return default_pixbuf;
		}

	}

	return limage->priv->pixbuf;

}


/*****************************************************************************/
/* Draw object method.                                                       */
/*****************************************************************************/
static void
draw_object (glLabelObject *object,
             cairo_t       *cr,
             gboolean       screen_flag,
             glMergeRecord *record)
{
        gdouble          x0, y0;
	gdouble          w, h;
	const GdkPixbuf *pixbuf;
	gint             image_w, image_h;

	gl_debug (DEBUG_LABEL, "START");

	gl_label_object_get_size (object, &w, &h);
        gl_label_object_get_position (object, &x0, &y0);

	pixbuf = gl_label_image_get_pixbuf (GL_LABEL_IMAGE (object), record);
	image_w = gdk_pixbuf_get_width (pixbuf);
	image_h = gdk_pixbuf_get_height (pixbuf);

	cairo_save (cr);

        cairo_rectangle (cr, 0.0, 0.0, w, h);

	cairo_scale (cr, w/image_w, h/image_h);
        gdk_cairo_set_source_pixbuf (cr, (GdkPixbuf *)pixbuf, 0, 0);
        cairo_fill (cr);

	cairo_restore (cr);

	gl_debug (DEBUG_LABEL, "END");
}


/*****************************************************************************/
/* Is object at coordinates?                                                 */
/*****************************************************************************/
static gboolean
object_at (glLabelObject *object,
           cairo_t       *cr,
           gdouble        x,
           gdouble        y)
{
        gdouble           w, h;

        gl_label_object_get_size (object, &w, &h);

        cairo_new_path (cr);
        cairo_rectangle (cr, 0.0, 0.0, w, h);

        if (cairo_in_fill (cr, x, y))
        {
                return TRUE;
        }

        return FALSE;
}




/*
 * Local Variables:       -- emacs
 * mode: C                -- emacs
 * c-basic-offset: 8      -- emacs
 * tab-width: 8           -- emacs
 * indent-tabs-mode: nil  -- emacs
 * End:                   -- emacs
 */
