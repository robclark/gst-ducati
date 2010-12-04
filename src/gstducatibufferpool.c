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

#include "gstducatibufferpool.h"

/*
 * GstDucatiBuffer
 */

static GstBufferClass *buffer_parent_class;

/* Get the original buffer, or whatever is the best output buffer.
 * Consumes the input reference, produces the output reference
 */
GstBuffer *
gst_ducati_buffer_get (GstDucatiBuffer * self)
{
  if (self->orig) {
    // TODO copy to orig buffer.. if needed.
    gst_buffer_unref (self->orig);
    self->orig = NULL;
  }
  return GST_BUFFER (self);
}

static GstDucatiBuffer *
gst_ducati_buffer_new (GstDucatiBufferPool * pool)
{
  GstDucatiBuffer *self = (GstDucatiBuffer *)
      gst_mini_object_new (GST_TYPE_DUCATIBUFFER);
  guint sz;

  GST_LOG_OBJECT (pool->element, "creating buffer %p in pool %p", self, pool);

  self->pool = (GstDucatiBufferPool *)
      gst_mini_object_ref (GST_MINI_OBJECT (pool));

  GST_BUFFER_DATA (self) =
      gst_ducati_alloc_2d (pool->padded_width, pool->padded_height, &sz);
  GST_BUFFER_SIZE (self) = sz;

  gst_buffer_set_caps (GST_BUFFER (self), pool->caps);

  return self;
}

static void
gst_ducati_buffer_finalize (GstDucatiBuffer * self)
{
  GstDucatiBufferPool *pool = self->pool;
  gboolean resuscitated = FALSE;

  GST_LOG_OBJECT (pool->element, "finalizing buffer %p", self);

  GST_DUCATI_BUFFERPOOL_LOCK (pool);
  if (pool->running) {
    resuscitated = TRUE;

    GST_LOG_OBJECT (pool->element, "reviving buffer %p", self);
    gst_buffer_ref (GST_BUFFER (self));

    /* insert self into freelist */
    self->next = pool->freelist;
    pool->freelist = self;
  } else {
    GST_LOG_OBJECT (pool->element, "the pool is shutting down");
  }
  GST_DUCATI_BUFFERPOOL_UNLOCK (pool);

  if (!resuscitated) {
    GST_LOG_OBJECT (pool->element,
        "buffer %p (data %p, len %u) not recovered, freeing",
        self, GST_BUFFER_DATA (self), GST_BUFFER_SIZE (self));
    MemMgr_Free ((void *) GST_BUFFER_DATA (self));
    GST_BUFFER_DATA (self) = NULL;
    gst_mini_object_unref (GST_MINI_OBJECT (pool));
    GST_MINI_OBJECT_CLASS (buffer_parent_class)->
        finalize (GST_MINI_OBJECT (self));
  }
}

static void
gst_ducati_buffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  buffer_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
        GST_DEBUG_FUNCPTR (gst_ducati_buffer_finalize);
}

GType
gst_ducati_buffer_get_type (void)
{
  static GType type;

  if (G_UNLIKELY (type == 0)) {
    static const GTypeInfo info = {
      .class_size = sizeof (GstBufferClass),
      .class_init = gst_ducati_buffer_class_init,
      .instance_size = sizeof (GstDucatiBuffer),
    };
    type = g_type_register_static (GST_TYPE_BUFFER,
        "GstDucatiBuffer", &info, 0);
  }
  return type;
}

/*
 * GstDucatiBufferPool
 */

static GstMiniObjectClass *bufferpool_parent_class = NULL;

/** create new bufferpool */
GstDucatiBufferPool *
gst_ducati_bufferpool_new (GstElement * element, GstCaps * caps)
{
  GstDucatiBufferPool *self = (GstDucatiBufferPool *)
      gst_mini_object_new (GST_TYPE_DUCATIBUFFERPOOL);
  GstStructure *s = gst_caps_get_structure (caps, 0);

  self->element = gst_object_ref (element);
  gst_structure_get_int (s, "width", &self->padded_width);
  gst_structure_get_int (s, "height", &self->padded_height);
  self->caps = gst_caps_ref (caps);
  self->freelist = NULL;
  self->lock = g_mutex_new ();
  self->running = TRUE;

  return self;
}

/** destroy existing bufferpool */
void
gst_ducati_bufferpool_destroy (GstDucatiBufferPool * self)
{
  g_return_if_fail (self);

  GST_DUCATI_BUFFERPOOL_LOCK (self);
  self->running = FALSE;
  GST_DUCATI_BUFFERPOOL_UNLOCK (self);

  GST_DEBUG_OBJECT (self->element, "destroy pool");

  /* free all buffers on the freelist */
  while (self->freelist) {
    GstDucatiBuffer *buf = self->freelist;
    self->freelist = buf->next;
    gst_buffer_unref (GST_BUFFER (buf));
  }

  gst_mini_object_unref (GST_MINI_OBJECT (self));
}

/** get buffer from bufferpool, allocate new buffer if needed */
GstDucatiBuffer *
gst_ducati_bufferpool_get (GstDucatiBufferPool * self, GstBuffer * orig)
{
  GstDucatiBuffer *buf = NULL;

  g_return_if_fail (self);

  GST_DUCATI_BUFFERPOOL_LOCK (self);
  if (self->running) {
    /* re-use a buffer off the freelist if any are available
     */
    if (self->freelist) {
      buf = self->freelist;
      self->freelist = buf->next;
    } else {
      buf = gst_ducati_buffer_new (self);
    }
    buf->orig = orig;
  }
  GST_DUCATI_BUFFERPOOL_UNLOCK (self);

  if (buf && orig) {
    GST_BUFFER_TIMESTAMP (buf) = GST_BUFFER_TIMESTAMP (orig);
    GST_BUFFER_DURATION (buf) = GST_BUFFER_DURATION (orig);
  }

  return buf;
}

static void
gst_ducati_bufferpool_finalize (GstDucatiBufferPool * self)
{
  g_mutex_free (self->lock);
  gst_caps_unref (self->caps);
  gst_object_unref (self->element);
  GST_MINI_OBJECT_CLASS (bufferpool_parent_class)->
      finalize (GST_MINI_OBJECT (self));
}

static void
gst_ducati_bufferpool_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  bufferpool_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
        GST_DEBUG_FUNCPTR (gst_ducati_bufferpool_finalize);
}

GType
gst_ducati_bufferpool_get_type (void)
{
  static GType type;

  if (G_UNLIKELY (type == 0)) {
    static const GTypeInfo info = {
      .class_size = sizeof (GstMiniObjectClass),
      .class_init = gst_ducati_bufferpool_class_init,
      .instance_size = sizeof (GstDucatiBufferPool),
    };
    type = g_type_register_static (GST_TYPE_MINI_OBJECT,
        "GstDucatiBufferPool", &info, 0);
  }
  return type;
}
