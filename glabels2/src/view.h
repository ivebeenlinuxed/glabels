/*
 *  (GLABELS) Label and Business Card Creation program for GNOME
 *
 *  view.h:  GLabels View module header file
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

#ifndef __VIEW_H__
#define __VIEW_H__

#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "label-object.h"

typedef enum {
	GL_VIEW_STATE_ARROW,
	GL_VIEW_STATE_OBJECT_CREATE
} glViewState;

#define GL_TYPE_VIEW            (gl_view_get_type ())
#define GL_VIEW(obj)            (GTK_CHECK_CAST((obj), GL_TYPE_VIEW, glView ))
#define GL_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GL_TYPE_VIEW, glViewClass))
#define GL_IS_VIEW(obj)         (GTK_CHECK_TYPE ((obj), GL_TYPE_VIEW))
#define GL_IS_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GL_TYPE_VIEW))

typedef struct _glView glView;
typedef struct _glViewClass glViewClass;

#include "view-object.h"

struct _glView {
	GtkVBox           parent_widget;

	glLabel           *label;

	GtkWidget         *canvas;
	gdouble           scale;
	gint              n_bg_items;
	GList             *bg_item_list;
	gint              n_fg_items;
	GList             *fg_item_list;

	glViewState       state;
	glLabelObjectType create_type;

	GList             *object_list;
	GList             *selected_object_list;

	gint              have_selection;
	glLabel           *selection_data;
	GtkWidget         *invisible;

	GtkWidget         *menu;
};

struct _glViewClass {
	GtkVBoxClass      parent_class;
};

extern guint     gl_view_get_type           (void);

extern GtkWidget *gl_view_new               (glLabel *label);

extern void      gl_view_raise_fg           (glView *view);

extern void      gl_view_arrow_mode         (glView *view);
extern void      gl_view_object_create_mode (glView *view,
					     glLabelObjectType type);

extern void      gl_view_select_object      (glView *view,
					     glViewObject *view_object);
extern void      gl_view_select_all         (glView *view);
extern void      gl_view_unselect_all       (glView *view);
extern void      gl_view_delete_selection   (glView *view);


extern int       gl_view_item_event_handler (GnomeCanvasItem *item,
					     GdkEvent        *event,
					     glViewObject    *view_object);

extern void      gl_view_cut                (glView *view);
extern void      gl_view_copy               (glView *view);
extern void      gl_view_paste              (glView *view);

extern void      gl_view_zoom_in            (glView *view);
extern void      gl_view_zoom_out           (glView *view);
extern void      gl_view_set_zoom           (glView *view, gdouble scale);
extern gdouble   gl_view_get_zoom           (glView *view);

#endif