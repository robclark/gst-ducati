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
 * SECTION:element-ducativp6dec
 *
 * FIXME:Describe ducativp6dec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ducativp6dec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstducativp6dec.h"


#define PADX  48
#define PADY  48


GST_BOILERPLATE (GstDucatiVP6Dec, gst_ducati_vp6dec, GstDucatiVidDec,
    GST_TYPE_DUCATIVIDDEC);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp6, "
        "width = (int)[ 16, 2048 ], "
        "height = (int)[ 16, 2048 ], "
        "framerate = (fraction)[ 0, max ];")
    );

/* GstDucatiVidDec vmethod implementations */

static void
gst_ducati_vp6dec_update_buffer_size (GstDucatiVidDec * self)
{
  gint w = self->width;
  gint h = self->height;

  /* calculate output buffer parameters: */
  self->padded_width = (w + (2 * PADX) + 0x7f) & ~0x7f;
  self->padded_height = h + 2 * PADY;
  self->min_buffers = 8;
}

static gboolean
gst_ducati_vp6dec_allocate_params (GstDucatiVidDec * self, gint params_sz,
    gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz)
{
  gboolean ret = parent_class->allocate_params (self,
      sizeof (Ivp6VDEC_Params), sizeof (Ivp6VDEC_DynamicParams),
      sizeof (Ivp6VDEC_Status), sizeof (Ivp6VDEC_InArgs),
      sizeof (Ivp6VDEC_OutArgs));

  if (ret) {
    Ivp6VDEC_Params *params = (Ivp6VDEC_Params *) self->params;
    self->params->displayDelay = IVIDDEC3_DECODE_ORDER;
    self->dynParams->newFrameFlag = FALSE;
    self->dynParams->lateAcquireArg = -1;
    self->params->numInputDataUnits = 1;
    self->params->numOutputDataUnits = 1;
    params->ivfFormat = FALSE;
  }

  return ret;
}

/* this should be unneeded in future codec versions: */
static GstBuffer *
gst_ducati_vp6dec_push_input (GstDucatiVidDec * vdec, GstBuffer * buf)
{
  guint32 sz;

  if (G_UNLIKELY (vdec->first_in_buffer) && vdec->codec_data) {
    // XXX none of the vp6 clips I've seen have codec_data..
  }

  /* current codec version requires size prepended on input buffer: */
  sz = GST_BUFFER_SIZE (buf);
  push_input (vdec, (guint8 *)&sz, 4);

  push_input (vdec, GST_BUFFER_DATA (buf), sz);
  gst_buffer_unref (buf);

  return NULL;
}


/* GObject vmethod implementations */

static void
gst_ducati_vp6dec_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "DucatiVP6Dec",
      "Codec/Decoder/Video",
      "Decodes video in On2 VP6 format with ducati",
      "Rob Clark <rob@ti.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_ducati_vp6dec_class_init (GstDucatiVP6DecClass * klass)
{
  GstDucatiVidDecClass *bclass = GST_DUCATIVIDDEC_CLASS (klass);
  bclass->codec_name = "ivahd_vp6dec";
  bclass->update_buffer_size =
      GST_DEBUG_FUNCPTR (gst_ducati_vp6dec_update_buffer_size);
  bclass->allocate_params =
      GST_DEBUG_FUNCPTR (gst_ducati_vp6dec_allocate_params);
  bclass->push_input =
      GST_DEBUG_FUNCPTR (gst_ducati_vp6dec_push_input);
}

static void
gst_ducati_vp6dec_init (GstDucatiVP6Dec * self,
    GstDucatiVP6DecClass * gclass)
{
}
