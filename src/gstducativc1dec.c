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
 * SECTION:element-ducativc1dec
 *
 * FIXME:Describe ducativc1dec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ducativc1dec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstducativc1dec.h"


#define PADX  32
#define PADY  40


GST_BOILERPLATE (GstDucatiVC1Dec, gst_ducati_vc1dec, GstDucatiVidDec,
    GST_TYPE_DUCATIVIDDEC);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-wmv, "
        "wmvversion = (int)[ 2, 3 ], "
        "format = (fourcc){ WVC1, WMV3, WMV2, WMV1 }, "
        "width = (int)[ 16, 2048 ], "
        "height = (int)[ 16, 2048 ], "
        "framerate = (fraction)[ 0, max ];")
    );

/* GstDucatiVidDec vmethod implementations */

static gboolean
gst_ducati_vc1dec_parse_caps (GstDucatiVidDec * vdec, GstStructure * s)
{
  GstDucatiVC1Dec *self = GST_DUCATIVC1DEC (vdec);

  if (parent_class->parse_caps (vdec, s)) {
    guint32 format;
    gboolean ret = gst_structure_get_fourcc (s, "format", &format);
    if (ret) {
      switch (format) {
        case GST_MAKE_FOURCC ('W', 'V', 'C', '1'):
          self->level = 4;
          break;
        case GST_MAKE_FOURCC ('W', 'M', 'V', '3'):
          self->level = 3;
          break;
        case GST_MAKE_FOURCC ('W', 'M', 'V', '2'):
          self->level = 2;
          break;
        case GST_MAKE_FOURCC ('W', 'M', 'V', '1'):
          self->level = 1;
          break;
        default:
          ret = FALSE;
          break;
      }
    }
    return ret;
  }

  return FALSE;
}

static void
gst_ducati_vc1dec_update_buffer_size (GstDucatiVidDec * self)
{
  gint w = self->width;
  gint h = self->height;

  /* calculate output buffer parameters: */
  self->padded_width = ALIGN2 (w + (2 * PADX), 7);
  self->padded_height = (ALIGN2 (h / 2, 4) * 2) + 2 * PADY;
  self->min_buffers = 8;
}

static gboolean
gst_ducati_vc1dec_allocate_params (GstDucatiVidDec * self, gint params_sz,
    gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz)
{
  gboolean ret = parent_class->allocate_params (self,
      sizeof (IVC1VDEC_Params), sizeof (IVC1VDEC_DynamicParams),
      sizeof (IVC1VDEC_Status), sizeof (IVC1VDEC_InArgs),
      sizeof (IVC1VDEC_OutArgs));

  if (ret) {
    IVC1VDEC_Params *params = (IVC1VDEC_Params *) self->params;
    self->params->maxBitRate = 45000000;
    self->params->displayDelay = IVIDDEC3_DISPLAY_DELAY_1;
    params->FrameLayerDataPresentFlag = FALSE;
  }

  return ret;
}

static GstBuffer *
gst_ducati_vc1dec_push_input (GstDucatiVidDec * vdec, GstBuffer * buf)
{
  GstDucatiVC1Dec *self = GST_DUCATIVC1DEC (vdec);

  if (G_UNLIKELY (vdec->first_in_buffer) && vdec->codec_data) {
    if (self->level == 4) {
      /* for VC-1 Advanced Profile, strip off first byte, and
       * send rest of codec_data unmodified;
       */
      push_input (vdec, GST_BUFFER_DATA (vdec->codec_data) + 1,
          GST_BUFFER_SIZE (vdec->codec_data) - 1);
    } else {
      guint32 val;

      /* for VC-1 Simple and Main Profile, build the Table 265 Sequence
       * Layer Data Structure header (refer to VC-1 spec, Annex L):
       */

      val = 0xc5ffffff;         /* we don't know the number of frames */
      push_input (vdec, (guint8 *) & val, 4);

      /* STRUCT_C (preceded by length).. see Table 263, 264 */
      val = GST_BUFFER_SIZE (vdec->codec_data);
      push_input (vdec, (guint8 *) & val, 4);
      push_input (vdec, GST_BUFFER_DATA (vdec->codec_data), val);

      /* STRUCT_A.. see Table 260 and Annex J.2 */
      push_input (vdec, (guint8 *) & vdec->height, 4);
      push_input (vdec, (guint8 *) & vdec->width, 4);

      val = 0x0000000c;
      push_input (vdec, (guint8 *) & val, 4);

      /* STRUCT_B.. see Table 261, 262 */
      val = 0x00000000;         /* not sure how to populate, but codec ignores anyways */
      push_input (vdec, (guint8 *) & val, 4);
      push_input (vdec, (guint8 *) & val, 4);
      push_input (vdec, (guint8 *) & val, 4);
    }
  }

  /* VC-1 Advanced profile needs start-code prepended: */
  if (self->level == 4) {
    static guint8 sc[] = { 0x00, 0x00, 0x01, 0x0d };    /* start code */
    push_input (vdec, sc, sizeof (sc));
  }

  push_input (vdec, GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));
  gst_buffer_unref (buf);

  return NULL;
}


/* GObject vmethod implementations */

static void
gst_ducati_vc1dec_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "DucatiVC1Dec",
      "Codec/Decoder/Video",
      "Decodes video in WMV/VC-1 format with ducati",
      "Rob Clark <rob@ti.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_ducati_vc1dec_class_init (GstDucatiVC1DecClass * klass)
{
  GstDucatiVidDecClass *bclass = GST_DUCATIVIDDEC_CLASS (klass);
  bclass->codec_name = "ivahd_vc1vdec";
  bclass->parse_caps =
      GST_DEBUG_FUNCPTR (gst_ducati_vc1dec_parse_caps);
  bclass->update_buffer_size =
      GST_DEBUG_FUNCPTR (gst_ducati_vc1dec_update_buffer_size);
  bclass->allocate_params =
      GST_DEBUG_FUNCPTR (gst_ducati_vc1dec_allocate_params);
  bclass->push_input =
      GST_DEBUG_FUNCPTR (gst_ducati_vc1dec_push_input);
}

static void
gst_ducati_vc1dec_init (GstDucatiVC1Dec * self,
    GstDucatiVC1DecClass * gclass)
{
}
