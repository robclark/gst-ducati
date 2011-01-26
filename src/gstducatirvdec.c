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
 * SECTION:element-ducatirvdec
 *
 * FIXME:Describe ducatirvdec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ducatirvdec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstducatirvdec.h"


#define PADX  32
#define PADY  32


GST_BOILERPLATE (GstDucatiRVDec, gst_ducati_rvdec, GstDucatiVidDec,
    GST_TYPE_DUCATIVIDDEC);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-pn-realvideo, "
        "systemstream = (boolean)false, "
        "rmversion = (int){ 3, 4 }, "
        "width = (int)[ 16, 2048 ], "
        "height = (int)[ 16, 2048 ], "
        "framerate = (fraction)[ 0, max ];")
    );

/* GstDucatiVidDec vmethod implementations */

static gboolean
gst_ducati_rvdec_parse_caps (GstDucatiVidDec * vdec, GstStructure * s)
{
  GstDucatiRVDec *self = GST_DUCATIRVDEC (vdec);

  if (parent_class->parse_caps (vdec, s)) {
    gboolean ret = gst_structure_get_int (s, "rmversion", &self->rmversion);
    if (ret) {
      IrealVDEC_Params *params = (IrealVDEC_Params *) vdec->params;
      GST_DEBUG_OBJECT (self, "rmversion: %d", self->rmversion);

      if (self->rmversion == 3) {
        params->stream_type = 1;
        params->codec_version = 8;
      } else if (self->rmversion == 4) {
        params->stream_type = 1;
        params->codec_version = 9;
      } else {
        ret = FALSE;
      }
    }

    return ret;
  }

  return FALSE;
}

static void
gst_ducati_rvdec_update_buffer_size (GstDucatiVidDec * self)
{
  /* calculate output buffer parameters: */
  self->padded_width = ALIGN2 (self->width + (2 * PADX), 7);
  self->padded_height = self->height + 2 * PADY;
  self->min_buffers = 8;
}

static gboolean
gst_ducati_rvdec_allocate_params (GstDucatiVidDec * vdec, gint params_sz,
    gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz)
{
  gboolean ret = parent_class->allocate_params (vdec,
      sizeof (IrealVDEC_Params), sizeof (IrealVDEC_DynamicParams),
      sizeof (IrealVDEC_Status), sizeof (IrealVDEC_InArgs),
      sizeof (IrealVDEC_OutArgs));

  if (ret) {
    /*IrealVDEC_Params *params = (IrealVDEC_Params *) vdec->params;*/
    vdec->params->displayDelay = IVIDDEC3_DISPLAY_DELAY_1;
    vdec->dynParams->newFrameFlag = FALSE;
    vdec->dynParams->lateAcquireArg = -1;
    vdec->params->numInputDataUnits = 1;
    vdec->params->numOutputDataUnits = 1;
  }

  return ret;
}

static GstBuffer *
gst_ducati_rvdec_push_input (GstDucatiVidDec * vdec, GstBuffer * buf)
{
  GstDucatiRVDec *self = GST_DUCATIRVDEC (vdec);
  guint8 *data;
  guint8 val[4];
  gint i, sz, slice_count;

  /* *** on first buffer, build up the stream header for the codec *** */
  if (G_UNLIKELY (vdec->first_in_buffer) && vdec->codec_data) {

    sz = GST_BUFFER_SIZE (vdec->codec_data);
    data = GST_BUFFER_DATA (vdec->codec_data);

    /* header size, 4 bytes, big-endian */
    GST_WRITE_UINT32_BE (val, sz + 26);
    push_input (vdec, val, 4);

    /* stream type */
    if (self->rmversion == 3) {
      push_input (vdec, (guint8 *)"VIDORV30", 8);
    } else if (self->rmversion == 4) {
      push_input (vdec, (guint8 *)"VIDORV40", 8);
    }

    /* horiz x vert resolution */
    GST_WRITE_UINT16_BE (val, vdec->width);
    push_input (vdec, val, 2);
    GST_WRITE_UINT16_BE (val, vdec->height);
    push_input (vdec, val, 2);

    /* unknown? */
    GST_WRITE_UINT32_BE (val, 0x000c0000);
    push_input (vdec, val, 4);

    /* unknown? may be framerate.. */
    GST_WRITE_UINT32_BE (val, 0x0000000f);
    push_input (vdec, val, 4);

    /* unknown? */
    GST_WRITE_UINT16_BE (val, 0x0000);
    push_input (vdec, val, 2);

    /* and rest of stream header is the codec_data */
    push_input (vdec, data, sz);
  }

  data = GST_BUFFER_DATA (buf);
  sz = GST_BUFFER_SIZE (buf);
  slice_count = (*data++) + 1;

  /* payload size, excluding fixed header and slice header */
  sz -= 1 + (8 * slice_count);

  /* *** insert frame header *** */
  /* payload size */
  GST_WRITE_UINT32_BE (val, sz);
  push_input (vdec, val, 4);

  /* unknown? may be timestamp, hopefully decoder doesn't care */
  GST_WRITE_UINT32_BE (val, 0x00000001);
  push_input (vdec, val, 4);

  /* unknown? may be sequence number, hopefully decoder doesn't care */
  GST_WRITE_UINT16_BE (val, 0x0000);
  push_input (vdec, val, 2);

  /* unknown? may indicate I frame, hopefully decoder doesn't care */
  if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT)) {
    GST_WRITE_UINT16_BE (val, 0x0000);
  } else {
    GST_WRITE_UINT16_BE (val, 0x0002);
  }
  push_input (vdec, val, 2);

  /* unknown? seems to be always zeros */
  GST_WRITE_UINT32_BE (val, 0x00000000);
  push_input (vdec, val, 4);

  /* convert the slice_header to big endian, and note that the codec
   * expects to get slice_count rather than slice_count-1
   */
  GST_WRITE_UINT32_BE (val, slice_count);
  push_input (vdec, val, 4);

  for (i = 0; i < slice_count; i++) {
    GST_WRITE_UINT32_BE (val, 0x00000001);
    push_input (vdec, val, 4);

    data += 4;
    GST_WRITE_UINT32_BE (val, GST_READ_UINT32_LE (data));
    data += 4;
    push_input (vdec, val, 4);
  }

  /* copy the payload (rest of buffer) */
  push_input (vdec, data, sz);
  gst_buffer_unref (buf);

  return NULL;
}

/* GObject vmethod implementations */

static void
gst_ducati_rvdec_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "DucatiRVDec",
      "Codec/Decoder/Video",
      "Decodes video in RealVideo (RV8/9/10) format with ducati",
      "Rob Clark <rob@ti.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_ducati_rvdec_class_init (GstDucatiRVDecClass * klass)
{
  GstDucatiVidDecClass *bclass = GST_DUCATIVIDDEC_CLASS (klass);
  bclass->codec_name = "ivahd_realvdec";
  bclass->parse_caps =
      GST_DEBUG_FUNCPTR (gst_ducati_rvdec_parse_caps);
  bclass->update_buffer_size =
      GST_DEBUG_FUNCPTR (gst_ducati_rvdec_update_buffer_size);
  bclass->allocate_params =
      GST_DEBUG_FUNCPTR (gst_ducati_rvdec_allocate_params);
  bclass->push_input =
      GST_DEBUG_FUNCPTR (gst_ducati_rvdec_push_input);
}

static void
gst_ducati_rvdec_init (GstDucatiRVDec * self,
    GstDucatiRVDecClass * gclass)
{
}
