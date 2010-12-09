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
 * SECTION:element-ducatih264dec
 *
 * FIXME:Describe ducatih264dec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ducatih264dec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstducatih264dec.h"


#define PADX  32
#define PADY  24


GST_BOILERPLATE (GstDucatiH264Dec, gst_ducati_h264dec, GstDucatiVidDec,
    GST_TYPE_DUCATIVIDDEC);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
//        "alignment = au, "              /* only entire frames */
//        "stream-format = byte-stream, " /* only byte-stream */
        "width = (int)[ 16, 2048 ], "
        "height = (int)[ 16, 2048 ], "
        "framerate = (fraction)[ 0, max ];")
    );

/* GstDucatiVidDec vmethod implementations */

static void
gst_ducati_h264dec_update_buffer_size (GstDucatiVidDec * self)
{
  gint w = self->width;
  gint h = self->height;

  /* calculate output buffer parameters: */
  self->padded_width = ALIGN2 (w + (2 * PADX), 7);
  self->padded_height = h + 4 * PADY;
  self->min_buffers = MIN (16, 32768 / ((w / 16) * (h / 16))) + 3;
}

static gboolean
gst_ducati_h264dec_allocate_params (GstDucatiVidDec * self, gint params_sz,
    gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz)
{
  gboolean ret = parent_class->allocate_params (self,
      sizeof (IH264VDEC_Params), sizeof (IH264VDEC_DynamicParams),
      sizeof (IH264VDEC_Status), sizeof (IH264VDEC_InArgs),
      sizeof (IH264VDEC_OutArgs));

  if (ret) {
    IH264VDEC_Params *params = (IH264VDEC_Params *) self->params;
    self->params->displayDelay = IVIDDEC3_DISPLAY_DELAY_AUTO;
    params->maxNumRefFrames = IH264VDEC_NUM_REFFRAMES_AUTO;
    params->pConstantMemory = 0;
    params->presetLevelIdc = IH264VDEC_LEVEL41;
    params->errConcealmentMode = IH264VDEC_APPLY_CONCEALMENT;
  }

  return ret;
}

/* GObject vmethod implementations */

static void
gst_ducati_h264dec_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "DucatiH264Dec",
      "Codec/Decoder/Video",
      "Decodes video in H.264/bytestream format with ducati",
      "Rob Clark <rob@ti.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_ducati_h264dec_class_init (GstDucatiH264DecClass * klass)
{
  GstDucatiVidDecClass *bclass = GST_DUCATIVIDDEC_CLASS (klass);
  bclass->codec_name = "ivahd_h264dec";
  bclass->update_buffer_size =
      GST_DEBUG_FUNCPTR (gst_ducati_h264dec_update_buffer_size);
  bclass->allocate_params =
      GST_DEBUG_FUNCPTR (gst_ducati_h264dec_allocate_params);
}

static void
gst_ducati_h264dec_init (GstDucatiH264Dec * self,
    GstDucatiH264DecClass * gclass)
{
}
