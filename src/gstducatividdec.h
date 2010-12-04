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

#ifndef __GST_DUCATIVIDDEC_H__
#define __GST_DUCATIVIDDEC_H__

#include <gst/gst.h>

#include "gstducati.h"
#include "gstducatibufferpool.h"


G_BEGIN_DECLS

// XXX move these
GST_DEBUG_CATEGORY_EXTERN (gst_ducati_debug);


#define GST_TYPE_DUCATIVIDDEC               (gst_ducati_viddec_get_type())
#define GST_DUCATIVIDDEC(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DUCATIVIDDEC, GstDucatiVidDec))
#define GST_DUCATIVIDDEC_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DUCATIVIDDEC, GstDucatiVidDecClass))
#define GST_IS_DUCATIVIDDEC(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DUCATIVIDDEC))
#define GST_IS_DUCATIVIDDEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DUCATIVIDDEC))
#define GST_DUCATIVIDDEC_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_DUCATIVIDDEC, GstDucatiVidDecClass))

typedef struct _GstDucatiVidDec      GstDucatiVidDec;
typedef struct _GstDucatiVidDecClass GstDucatiVidDecClass;

struct _GstDucatiVidDec
{
  GstElement parent;

  GstPad *sinkpad, *srcpad;

  GstDucatiBufferPool *pool;

  /* minimum output size required by the codec: */
  gint outsize;

  /* minimum number of buffers required by the codec: */
  gint min_buffers;

  /* input (unpadded) size of video: */
  gint width, height;

  /* output (padded) size including any codec padding: */
  gint padded_width, padded_height;

  /* output stride (>= padded_width) */
  gint stride;

  /* input buffer, allocated when codec is created: */
  guint8 *input;

  /* number of bytes pushed to input on current frame: */
  gint in_size;

  /* on first output buffer, we need to send crop info to sink.. and some
   * operations like flushing should be avoided if we haven't sent any
   * input buffers:
   */
  gboolean first_out_buffer, first_in_buffer;

  /* by default, codec_data from sinkpad is prepended to first buffer: */
  GstBuffer *codec_data;

  Engine_Handle           engine;
  VIDDEC3_Handle          codec;
  VIDDEC3_Params         *params;
  VIDDEC3_DynamicParams  *dynParams;
  VIDDEC3_Status         *status;
  XDM2_BufDesc           *inBufs;
  XDM2_BufDesc           *outBufs;
  VIDDEC3_InArgs         *inArgs;
  VIDDEC3_OutArgs        *outArgs;
};

struct _GstDucatiVidDecClass
{
  GstElementClass parent_class;

  const gchar *codec_name;

  /**
   * Parse codec specific fields the given caps structure.  The base-
   * class implementation of this method handles standard stuff like
   * width/height/framerate/codec_data.
   */
  gboolean (*parse_caps) (GstDucatiVidDec * self, GstStructure * s);

  /**
   * Called when the input buffer size changes, to recalculate codec required
   * output buffer size and minimum count
   */
  void (*update_buffer_size) (GstDucatiVidDec * self);

  /**
   * Called to allocate/initialize  params/dynParams/status/inArgs/outArgs
   */
  gboolean (*allocate_params) (GstDucatiVidDec * self, gint params_sz,
      gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz);

  /**
   * Push input data into codec's input buffer, returning a sub-buffer of
   * any remaining data, or NULL if none.  Consumes reference to 'buf'
   */
  GstBuffer * (*push_input) (GstDucatiVidDec * self, GstBuffer * buf);
};

GType gst_ducati_viddec_get_type (void);

/* helper methods for derived classes: */

static inline void
push_input (GstDucatiVidDec * self, guint8 *in, gint sz)
{
  GST_DEBUG_OBJECT (self, "push: %d bytes)", sz);
  memcpy (self->input + self->in_size, in, sz);
  self->in_size += sz;
}

G_END_DECLS

#endif /* __GST_DUCATIVIDDEC_H__ */
