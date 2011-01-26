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

/**
 * SECTION:element-ducatimpeg2dec
 *
 * FIXME:Describe ducatimpeg2dec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ducatimpeg2dec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstducatimpeg2dec.h"

GST_BOILERPLATE (GstDucatiMpeg2Dec, gst_ducati_mpeg2dec, GstDucatiVidDec,
    GST_TYPE_DUCATIVIDDEC);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int)[ 1, 2 ], "  // XXX check on MPEG-1..
        "systemstream = (boolean)false, "
        "width = (int)[ 64, 2048 ], "
        "height = (int)[ 64, 2048 ], "
        "framerate = (fraction)[ 0, max ];")
    );

/* GstDucatiVidDec vmethod implementations */

static void
gst_ducati_mpeg2dec_update_buffer_size (GstDucatiVidDec * self)
{
  gint w = self->width;
  gint h = self->height;

  /* calculate output buffer parameters: */
  self->padded_width = ALIGN2 (w, 7);
  self->padded_height = h;
  self->min_buffers = 8;
}

static gboolean
gst_ducati_mpeg2dec_allocate_params (GstDucatiVidDec * self, gint params_sz,
    gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz)
{
  gboolean ret = parent_class->allocate_params (self,
      sizeof (IMPEG2VDEC_Params), sizeof (IMPEG2VDEC_DynamicParams),
      sizeof (IMPEG2VDEC_Status), sizeof (IMPEG2VDEC_InArgs),
      sizeof (IMPEG2VDEC_OutArgs));

  if (ret) {
//    IMPEG2VDEC_Params *params = (IMPEG2VDEC_Params *) self->params;
    self->params->displayDelay = IVIDDEC3_DISPLAY_DELAY_AUTO;
  }

  return ret;
}

static GstBuffer *
gst_ducati_mpeg2dec_push_input (GstDucatiVidDec * vdec, GstBuffer * buf)
{
  GstDucatiMpeg2Dec *self = GST_DUCATIMPEG2DEC (vdec);
  guint sz = GST_BUFFER_SIZE (buf);
  guint8 *data = GST_BUFFER_DATA (buf);

  /* skip codec_data, which is same as first buffer from mpegvideoparse (and
   * appears to be periodically resent) and instead prepend to next frame..
   */
  if (vdec->codec_data && (sz == GST_BUFFER_SIZE (vdec->codec_data)) &&
      !memcmp (data, GST_BUFFER_DATA (vdec->codec_data), sz)) {
    GST_DEBUG_OBJECT (self, "skipping codec_data buffer");
    self->prepend_codec_data = TRUE;
  } else {
    if (self->prepend_codec_data) {
      GST_DEBUG_OBJECT (self, "prepending codec_data buffer");
      push_input (vdec, GST_BUFFER_DATA (vdec->codec_data),
          GST_BUFFER_SIZE (vdec->codec_data));
      self->prepend_codec_data = FALSE;
    }
    push_input (vdec, data, sz);
  }

  gst_buffer_unref (buf);

  return NULL;
}

/* GObject vmethod implementations */

static void
gst_ducati_mpeg2dec_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "DucatiMpeg2Dec",
      "Codec/Decoder/Video",
      "Decodes video in MPEG-2 format with ducati",
      "Rob Clark <rob@ti.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_ducati_mpeg2dec_class_init (GstDucatiMpeg2DecClass * klass)
{
  GstDucatiVidDecClass *bclass = GST_DUCATIVIDDEC_CLASS (klass);
  bclass->codec_name = "ivahd_mpeg2vdec";
  bclass->update_buffer_size =
      GST_DEBUG_FUNCPTR (gst_ducati_mpeg2dec_update_buffer_size);
  bclass->allocate_params =
      GST_DEBUG_FUNCPTR (gst_ducati_mpeg2dec_allocate_params);
  bclass->push_input =
      GST_DEBUG_FUNCPTR (gst_ducati_mpeg2dec_push_input);
}

static void
gst_ducati_mpeg2dec_init (GstDucatiMpeg2Dec * self,
    GstDucatiMpeg2DecClass * gclass)
{
}
