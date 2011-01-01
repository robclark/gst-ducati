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
 * SECTION:element-ducativp7dec
 *
 * FIXME:Describe ducativp7dec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ducativp7dec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstducativp7dec.h"


#define PADX  48
#define PADY  48


GST_BOILERPLATE (GstDucatiVP7Dec, gst_ducati_vp7dec, GstDucatiVidDec,
    GST_TYPE_DUCATIVIDDEC);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp7, "
        "width = (int)[ 16, 2048 ], "
        "height = (int)[ 16, 2048 ], "
        "framerate = (fraction)[ 0, max ];")
    );

/* GstDucatiVidDec vmethod implementations */

static void
gst_ducati_vp7dec_update_buffer_size (GstDucatiVidDec * self)
{
  gint w = self->width;
  gint h = self->height;

  /* calculate output buffer parameters: */
  self->padded_width = ALIGN2 (w + (2 * PADX), 7);
  self->padded_height = h + 2 * PADY;
  self->min_buffers = 8;
}

static gboolean
gst_ducati_vp7dec_allocate_params (GstDucatiVidDec * self, gint params_sz,
    gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz)
{
  gboolean ret = parent_class->allocate_params (self,
      sizeof (Ivp7VDEC_Params), sizeof (Ivp7VDEC_DynamicParams),
      sizeof (Ivp7VDEC_Status), sizeof (Ivp7VDEC_InArgs),
      sizeof (Ivp7VDEC_OutArgs));

  if (ret) {
    Ivp7VDEC_Params *params = (Ivp7VDEC_Params *) self->params;
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
gst_ducati_vp7dec_push_input (GstDucatiVidDec * vdec, GstBuffer * buf)
{
  guint32 sz;

  if (G_UNLIKELY (vdec->first_in_buffer) && vdec->codec_data) {
    // XXX none of the vp7 clips I've seen have codec_data..
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
gst_ducati_vp7dec_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "DucatiVP7Dec",
      "Codec/Decoder/Video",
      "Decodes video in On2 VP7 format with ducati",
      "Rob Clark <rob@ti.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_ducati_vp7dec_class_init (GstDucatiVP7DecClass * klass)
{
  GstDucatiVidDecClass *bclass = GST_DUCATIVIDDEC_CLASS (klass);
  bclass->codec_name = "ivahd_vp7dec";
  bclass->update_buffer_size =
      GST_DEBUG_FUNCPTR (gst_ducati_vp7dec_update_buffer_size);
  bclass->allocate_params =
      GST_DEBUG_FUNCPTR (gst_ducati_vp7dec_allocate_params);
  bclass->push_input =
      GST_DEBUG_FUNCPTR (gst_ducati_vp7dec_push_input);
}

static void
gst_ducati_vp7dec_init (GstDucatiVP7Dec * self,
    GstDucatiVP7DecClass * gclass)
{
}
