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
 * SECTION:element-ducatimpeg4dec
 *
 * FIXME:Describe ducatimpeg4dec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ducatimpeg4dec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstducatimpeg4dec.h"


#define PADX  32
#define PADY  32


GST_BOILERPLATE (GstDucatiMpeg4Dec, gst_ducati_mpeg4dec, GstDucatiVidDec,
    GST_TYPE_DUCATIVIDDEC);

#define MPEG4DEC_SINKCAPS_COMMON \
    "width = (int)[ 16, 2048 ], " \
    "height = (int)[ 16, 2048 ], " \
    "framerate = (fraction)[ 0, max ]"

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "video/mpeg, "
          "mpegversion = (int)4, "
          "systemstream = (boolean)false, "
          MPEG4DEC_SINKCAPS_COMMON ";"
        "video/x-divx, "
          "divxversion = (int)[4, 5], "  /* TODO check this */
          MPEG4DEC_SINKCAPS_COMMON ";"
        "video/x-xvid, "
          MPEG4DEC_SINKCAPS_COMMON ";"
        "video/x-3ivx, "
          MPEG4DEC_SINKCAPS_COMMON ";"
        )
    );

/* GstDucatiVidDec vmethod implementations */

static void
gst_ducati_mpeg4dec_update_buffer_size (GstDucatiVidDec * self)
{
  gint w = self->width;
  gint h = self->height;

  /* calculate output buffer parameters: */
  self->padded_width = ALIGN2 (w + PADX, 7);
  self->padded_height = h + PADY;
  self->min_buffers = 8;
}

static gboolean
gst_ducati_mpeg4dec_allocate_params (GstDucatiVidDec * self, gint params_sz,
    gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz)
{
  gboolean ret = parent_class->allocate_params (self,
      sizeof (IMPEG4VDEC_Params), sizeof (IMPEG4VDEC_DynamicParams),
      sizeof (IMPEG4VDEC_Status), sizeof (IMPEG4VDEC_InArgs),
      sizeof (IMPEG4VDEC_OutArgs));

  if (ret) {
    IMPEG4VDEC_Params *params = (IMPEG4VDEC_Params *) self->params;
    self->params->displayDelay = IVIDDEC3_DISPLAY_DELAY_1;
    self->dynParams->lateAcquireArg = -1;
    params->outloopDeBlocking = TRUE;
    params->sorensonSparkStream = FALSE;
    params->ErrorConcealmentON = FALSE;
  }

  return ret;
}

/* GObject vmethod implementations */

static void
gst_ducati_mpeg4dec_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "DucatiMpeg4Dec",
      "Codec/Decoder/Video",
      "Decodes video in MPEG-4 format with ducati",
      "Rob Clark <rob@ti.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_ducati_mpeg4dec_class_init (GstDucatiMpeg4DecClass * klass)
{
  GstDucatiVidDecClass *bclass = GST_DUCATIVIDDEC_CLASS (klass);
  bclass->codec_name = "ivahd_mpeg4dec";
  bclass->update_buffer_size =
      GST_DEBUG_FUNCPTR (gst_ducati_mpeg4dec_update_buffer_size);
  bclass->allocate_params =
      GST_DEBUG_FUNCPTR (gst_ducati_mpeg4dec_allocate_params);
}

static void
gst_ducati_mpeg4dec_init (GstDucatiMpeg4Dec * self,
    GstDucatiMpeg4DecClass * gclass)
{
}
