/*
 * GStreamer
 * Copyright (c) 2010, Texas Instruments Incorporated
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GSTDUCATIBUFFERPOOL_H__
#define __GSTDUCATIBUFFERPOOL_H__

#include "gstducati.h"

G_BEGIN_DECLS

GType gst_ducati_buffer_get_type (void);
#define GST_TYPE_DUCATIBUFFER (gst_ducati_buffer_get_type())
#define GST_IS_DUCATIBUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATIBUFFER))
#define GST_DUCATIBUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATIBUFFER, GstDucatiBuffer))

GType gst_ducati_bufferpool_get_type (void);
#define GST_TYPE_DUCATIBUFFERPOOL (gst_ducati_bufferpool_get_type())
#define GST_IS_DUCATIBUFFERPOOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATIBUFFERPOOL))
#define GST_DUCATIBUFFERPOOL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATIBUFFERPOOL, GstDucatiBufferPool))

typedef struct _GstDucatiBufferPool GstDucatiBufferPool;
typedef struct _GstDucatiBuffer GstDucatiBuffer;

struct _GstDucatiBufferPool
{
  GstMiniObject parent;

  /* output (padded) size including any codec padding: */
  gint padded_width, padded_height;

  GstCaps         *caps;
  GMutex          *lock;
  gboolean         running;  /* with lock */
  GstElement      *element;  /* the element that owns us.. */
  GstDucatiBuffer *freelist; /* list of available buffers */
};

GstDucatiBufferPool * gst_ducati_bufferpool_new (GstElement * element, GstCaps * caps);
void gst_ducati_bufferpool_destroy (GstDucatiBufferPool * pool);
GstDucatiBuffer * gst_ducati_bufferpool_get (GstDucatiBufferPool * self, GstBuffer * orig);

#define GST_DUCATI_BUFFERPOOL_LOCK(self)     g_mutex_lock ((self)->lock)
#define GST_DUCATI_BUFFERPOOL_UNLOCK(self)   g_mutex_unlock ((self)->lock)

struct _GstDucatiBuffer {
  GstBuffer parent;

  GstDucatiBufferPool *pool; /* buffer-pool that this buffer belongs to */
  GstBuffer       *orig;     /* original buffer, if we need to copy output */
  GstDucatiBuffer *next;     /* next in freelist, if not in use */
};

GstBuffer * gst_ducati_buffer_get (GstDucatiBuffer * self);

G_END_DECLS

#endif /* __GSTDUCATIBUFFERPOOL_H__ */
