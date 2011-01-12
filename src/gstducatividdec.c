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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstducatividdec.h"

GST_BOILERPLATE (GstDucatiVidDec, gst_ducati_viddec, GstElement,
    GST_TYPE_ELEMENT);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED ("NV12", "[ 0, max ]"))
    );

enum
{
  PROP_0,
  PROP_VERSION,
};

/* helper functions */

static void
engine_close (GstDucatiVidDec * self)
{
  if (self->engine) {
    Engine_close (self->engine);
    self->engine = NULL;
  }

  if (self->params) {
    dce_free (self->params);
    self->params = NULL;
  }

  if (self->dynParams) {
    dce_free (self->dynParams);
    self->dynParams = NULL;
  }

  if (self->status) {
    dce_free (self->status);
    self->status = NULL;
  }

  if (self->inBufs) {
    dce_free (self->inBufs);
    self->inBufs = NULL;
  }

  if (self->outBufs) {
    dce_free (self->outBufs);
    self->outBufs = NULL;
  }

  if (self->inArgs) {
    dce_free (self->inArgs);
    self->inArgs = NULL;
  }

  if (self->outArgs) {
    dce_free (self->outArgs);
    self->outArgs = NULL;
  }
}

static gboolean
engine_open (GstDucatiVidDec * self)
{
  gboolean ret;

  if (G_UNLIKELY (self->engine)) {
    return TRUE;
  }

  GST_DEBUG_OBJECT (self, "opening engine");

  self->engine = Engine_open ("ivahd_vidsvr", NULL, NULL);
  if (G_UNLIKELY (!self->engine)) {
    GST_ERROR_OBJECT (self, "could not create engine");
    return FALSE;
  }

  ret = GST_DUCATIVIDDEC_GET_CLASS (self)->allocate_params (self,
      sizeof (IVIDDEC3_Params), sizeof (IVIDDEC3_DynamicParams),
      sizeof (IVIDDEC3_Status), sizeof (IVIDDEC3_InArgs),
      sizeof (IVIDDEC3_OutArgs));

  return ret;
}

static void
codec_delete (GstDucatiVidDec * self)
{
  if (self->pool) {
    gst_ducati_bufferpool_destroy (self->pool);
    self->pool = NULL;
  }

  if (self->codec) {
    VIDDEC3_delete(self->codec);
    self->codec = NULL;
  }

  if (self->input) {
    MemMgr_Free (self->input);
    self->input = NULL;
  }
}

static gboolean
codec_create (GstDucatiVidDec * self)
{
  gint err;
  const gchar *codec_name;

  codec_delete (self);

  if (G_UNLIKELY (!self->engine)) {
    GST_ERROR_OBJECT (self, "no engine");
    return FALSE;
  }

  /* these need to be set before VIDDEC3_create */
  self->params->maxWidth = self->width;
  self->params->maxHeight = self->height;

  codec_name = GST_DUCATIVIDDEC_GET_CLASS (self)->codec_name;

  /* create codec: */
  GST_DEBUG_OBJECT (self, "creating codec: %s", codec_name);
  self->codec = VIDDEC3_create (self->engine, (char *)codec_name, self->params);

  if (!self->codec) {
    return FALSE;
  }

  err = VIDDEC3_control (self->codec, XDM_SETPARAMS, self->dynParams, self->status);
  if (err) {
    GST_ERROR_OBJECT (self, "failed XDM_SETPARAMS");
    return FALSE;
  }

  self->first_in_buffer = TRUE;
  self->first_out_buffer = TRUE;

  /* allocate input buffer and initialize inBufs: */
  self->inBufs->numBufs = 1;
  self->input = gst_ducati_alloc_1d (self->width * self->height);
  self->inBufs->descs[0].buf = (XDAS_Int8 *) TilerMem_VirtToPhys (self->input);
  self->inBufs->descs[0].memType = XDM_MEMTYPE_RAW;

  return TRUE;
}

static inline GstBuffer *
codec_bufferpool_get (GstDucatiVidDec * self, GstBuffer * buf)
{
  if (G_UNLIKELY (!self->pool)) {
    GST_DEBUG_OBJECT (self, "creating bufferpool");
    self->pool = gst_ducati_bufferpool_new (GST_ELEMENT (self),
        GST_PAD_CAPS (self->srcpad));
  }
  return GST_BUFFER (gst_ducati_bufferpool_get (self->pool, buf));
}

static XDAS_Int32
codec_prepare_outbuf (GstDucatiVidDec * self, GstBuffer * buf)
{
  XDAS_Int16 y_type, uv_type;
  guint8 *y_vaddr, *uv_vaddr;
  SSPtr y_paddr, uv_paddr;

  y_vaddr = GST_BUFFER_DATA (buf);
  uv_vaddr = y_vaddr + self->stride * self->padded_height;

  y_paddr = TilerMem_VirtToPhys (y_vaddr);
  uv_paddr = TilerMem_VirtToPhys (uv_vaddr);

  y_type = gst_ducati_get_mem_type (y_paddr);
  uv_type = gst_ducati_get_mem_type (uv_paddr);

  if ((y_type < 0) || (uv_type < 0)) {
    GST_DEBUG_OBJECT (self, "non TILER buffer, fallback to bufferpool");
    return codec_prepare_outbuf (self, codec_bufferpool_get (self, buf));
  }

  if (!self->outBufs->numBufs) {
    /* initialize output buffer type */
    self->outBufs->numBufs = 2;
    self->outBufs->descs[0].memType = y_type;
    self->outBufs->descs[0].bufSize.tileMem.width = self->padded_width;
    self->outBufs->descs[0].bufSize.tileMem.height = self->padded_height;
    self->outBufs->descs[1].memType = uv_type;
    /* note that UV interleaved width is same a Y: */
    self->outBufs->descs[1].bufSize.tileMem.width = self->padded_width;
    self->outBufs->descs[1].bufSize.tileMem.height = self->padded_height / 2;
  } else {
    /* verify output buffer type matches what we've already given
     * to the codec
     */
    if ((self->outBufs->descs[0].memType != y_type) ||
        (self->outBufs->descs[1].memType != uv_type)) {
      GST_DEBUG_OBJECT (self, "buffer mismatch, fallback to bufferpool");
      return codec_prepare_outbuf (self, codec_bufferpool_get (self, buf));
    }
  }

  self->outBufs->descs[0].buf = (XDAS_Int8 *) y_paddr;
  self->outBufs->descs[1].buf = (XDAS_Int8 *) uv_paddr;

  return (XDAS_Int32) buf;      // XXX use lookup table
}

static GstBuffer *
codec_get_outbuf (GstDucatiVidDec * self, XDAS_Int32 id)
{
  GstBuffer *buf = (GstBuffer *) id;    // XXX use lookup table
  if (buf) {
    gst_buffer_ref (buf);
  }
  return buf;
}

static void
codec_unlock_outbuf (GstDucatiVidDec * self, XDAS_Int32 id)
{
  GstBuffer *buf = (GstBuffer *) id;    // XXX use lookup table
  if (buf) {
    GST_DEBUG_OBJECT (self, "free buffer: %d %p", id, buf);
    gst_buffer_unref (buf);
  }
}

static gint
codec_process (GstDucatiVidDec * self, gboolean send, gboolean flush)
{
  gint err;
  GstClockTime t;
  GstBuffer *outbuf = NULL;
  gint i;

  self->outArgs->outputID[0] = 0;
  self->outArgs->freeBufID[0] = 0;

  t = gst_util_get_timestamp ();
  err = VIDDEC3_process (self->codec,
      self->inBufs, self->outBufs, self->inArgs, self->outArgs);
  GST_INFO_OBJECT (self, "%10dns", (gint) (gst_util_get_timestamp () - t));

  if (err) {
    GST_WARNING_OBJECT (self, "err=%d, extendedError=%08x",
        err, self->outArgs->extendedError);

    err = VIDDEC3_control (self->codec, XDM_GETSTATUS,
        self->dynParams, self->status);

    GST_WARNING_OBJECT (self, "XDM_GETSTATUS: err=%d, extendedError=%08x",
        err, self->status->extendedError);

    if (XDM_ISFATALERROR (self->outArgs->extendedError) || flush) {
      /* we are processing for display and it is a non-fatal error, so lets
       * try to recover.. otherwise return the error
       */
      err = XDM_EFAIL;
    }
  }

  for (i = 0; self->outArgs->outputID[i]; i++) {
    if (G_UNLIKELY (self->first_out_buffer) && send) {
      /* send region of interest to sink on first buffer: */
      XDM_Rect *r = &(self->outArgs->displayBufs.bufDesc[0].activeFrameRegion);

      GST_DEBUG_OBJECT (self, "setting crop to %d, %d, %d, %d",
          r->topLeft.x, r->topLeft.y, r->bottomRight.x, r->bottomRight.y);

      gst_pad_push_event (self->srcpad,
          gst_event_new_crop (r->topLeft.y, r->topLeft.x,
              r->bottomRight.x - r->topLeft.x,
              r->bottomRight.y - r->topLeft.y));

      self->first_out_buffer = FALSE;
    }

    outbuf = codec_get_outbuf (self, self->outArgs->outputID[i]);
    if (send) {
      if (GST_IS_DUCATIBUFFER (outbuf)) {
        outbuf = gst_ducati_buffer_get (GST_DUCATIBUFFER (outbuf));
      }
      GST_DEBUG_OBJECT (self, "got buffer: %d %p (%" GST_TIME_FORMAT ")",
          i, outbuf, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)));
      gst_pad_push (self->srcpad, outbuf);
    } else {
      GST_DEBUG_OBJECT (self, "free buffer: %d %p", i, outbuf);
      gst_buffer_unref (outbuf);
    }
  }

  for (i = 0; self->outArgs->freeBufID[i]; i++) {
    codec_unlock_outbuf (self, self->outArgs->freeBufID[i]);
  }

  return err;
}

/** call control(FLUSH), and then process() to pop out all buffers */
static gboolean
codec_flush (GstDucatiVidDec * self, gboolean eos)
{
  gint err;

  GST_DEBUG_OBJECT (self, "flush: eos=%d", eos);

  /* note: flush is synchronized against _chain() to avoid calling
   * the codec from multiple threads
   */
  GST_PAD_STREAM_LOCK (self->sinkpad);

  if (G_UNLIKELY (self->first_in_buffer)) {
    return TRUE;
  }

  if (G_UNLIKELY (!self->codec)) {
    GST_WARNING_OBJECT (self, "no codec");
    return TRUE;
  }

  err = VIDDEC3_control (self->codec, XDM_FLUSH,
      self->dynParams, self->status);
  if (err) {
    GST_ERROR_OBJECT (self, "failed XDM_FLUSH");
    goto out;
  }

  self->inBufs->descs[0].bufSize.bytes = 0;
  self->inArgs->numBytes = 0;
  self->inArgs->inputID = 0;

  do {
    err = codec_process (self, eos, TRUE);
  } while (err != XDM_EFAIL);

  /* on a flush, it is normal (and not an error) for the last _process() call
   * to return an error..
   */
  err = XDM_EOK;

out:
  GST_PAD_STREAM_UNLOCK (self->sinkpad);
  GST_DEBUG_OBJECT (self, "done");

  return !err;
}

/* GstDucatiVidDec vmethod default implementations */

static gboolean
gst_ducati_viddec_parse_caps (GstDucatiVidDec * self, GstStructure * s)
{
  const GValue *codec_data;
  gint w, h;

  if (gst_structure_get_int (s, "width", &w) &&
      gst_structure_get_int (s, "height", &h)) {

    h = ALIGN2 (h, 4);                 /* round up to MB */
    w = ALIGN2 (w, 4);                 /* round up to MB */

    /* if we've already created codec, but the resolution has changed, we
     * need to re-create the codec:
     */
    if (G_UNLIKELY (self->codec)) {
      if ((h != self->height) || (w != self->width)) {
        codec_delete (self);
      }
    }

    self->width  = w;
    self->height = h;

    const GValue *codec_data = gst_structure_get_value (s, "codec_data");

    if (codec_data) {
      GstBuffer *buffer = gst_value_get_buffer (codec_data);
      GST_DEBUG_OBJECT (self, "codec_data: %" GST_PTR_FORMAT, buffer);
      self->codec_data = gst_buffer_ref (buffer);
    }

    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_ducati_viddec_allocate_params (GstDucatiVidDec * self, gint params_sz,
    gint dynparams_sz, gint status_sz, gint inargs_sz, gint outargs_sz)
{

  /* allocate params: */
  self->params = dce_alloc (params_sz);
  if (G_UNLIKELY (!self->params)) {
    return FALSE;
  }
  self->params->size = params_sz;
  self->params->maxFrameRate = 30000;
  self->params->maxBitRate = 10000000;

  self->params->dataEndianness = XDM_BYTE;
  self->params->forceChromaFormat = XDM_YUV_420SP;
  self->params->operatingMode = IVIDEO_DECODE_ONLY;

  self->params->displayBufsMode = IVIDDEC3_DISPLAYBUFS_EMBEDDED;
  self->params->inputDataMode = IVIDEO_ENTIREFRAME;
  self->params->outputDataMode = IVIDEO_ENTIREFRAME;
  self->params->numInputDataUnits = 0;
  self->params->numOutputDataUnits = 0;

  self->params->metadataType[0] = IVIDEO_METADATAPLANE_NONE;
  self->params->metadataType[1] = IVIDEO_METADATAPLANE_NONE;
  self->params->metadataType[2] = IVIDEO_METADATAPLANE_NONE;
  self->params->errorInfoMode = IVIDEO_ERRORINFO_OFF;

  /* allocate dynParams: */
  self->dynParams = dce_alloc (dynparams_sz);
  if (G_UNLIKELY (!self->dynParams)) {
    return FALSE;
  }
  self->dynParams->size = dynparams_sz;
  self->dynParams->decodeHeader = XDM_DECODE_AU;
  self->dynParams->displayWidth = 0;
  self->dynParams->frameSkipMode = IVIDEO_NO_SKIP;
  self->dynParams->newFrameFlag = XDAS_TRUE;

  /* allocate status: */
  self->status = dce_alloc (status_sz);
  if (G_UNLIKELY (!self->status)) {
    return FALSE;
  }
  self->status->size = status_sz;

  /* allocate inBufs/outBufs: */
  self->inBufs = dce_alloc (sizeof (XDM2_BufDesc));
  self->outBufs = dce_alloc (sizeof (XDM2_BufDesc));
  if (G_UNLIKELY (!self->inBufs) || G_UNLIKELY (!self->outBufs)) {
    return FALSE;
  }

  /* allocate inArgs/outArgs: */
  self->inArgs = dce_alloc (inargs_sz);
  self->outArgs = dce_alloc (outargs_sz);
  if (G_UNLIKELY (!self->inArgs) || G_UNLIKELY (!self->outArgs)) {
    return FALSE;
  }
  self->inArgs->size = inargs_sz;
  self->outArgs->size = outargs_sz;
}

static GstBuffer *
gst_ducati_viddec_push_input (GstDucatiVidDec * self, GstBuffer * buf)
{
  if (G_UNLIKELY (self->first_in_buffer) && self->codec_data) {
    push_input (self, GST_BUFFER_DATA (self->codec_data),
        GST_BUFFER_SIZE (self->codec_data));
  }

  /* just copy entire buffer */
  push_input (self, GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));
  gst_buffer_unref (buf);

  return NULL;
}

/* GstElement vmethod implementations */

static gboolean
gst_ducati_viddec_set_caps (GstPad * pad, GstCaps * caps)
{
  gboolean ret = TRUE;
  GstDucatiVidDec *self = GST_DUCATIVIDDEC (gst_pad_get_parent (pad));
  GstDucatiVidDecClass *klass = GST_DUCATIVIDDEC_GET_CLASS (self);
  GstStructure *s;

  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  s = gst_caps_get_structure (caps, 0);

  if (pad == self->sinkpad) {
    gint frn = 0, frd = 1;
    GST_INFO_OBJECT (self, "setcaps (sink): %" GST_PTR_FORMAT, caps);

    if (klass->parse_caps (self, s)) {
      GstCaps *outcaps;
      gboolean interlaced = FALSE;

      gst_structure_get_fraction (s, "framerate", &frn, &frd);

      self->stride = 4096;      /* TODO: don't hardcode */

      gst_structure_get_boolean (s, "interlaced", &interlaced);

      /* update output/padded sizes:
       */
      klass->update_buffer_size (self);

      self->outsize =
          GST_ROUND_UP_2 (self->stride * self->padded_height * 3) / 2;

      outcaps = gst_caps_new_simple ("video/x-raw-yuv-strided",
          "rowstride", G_TYPE_INT, self->stride,
          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('N','V','1','2'),
          "width", G_TYPE_INT, self->padded_width,
          "height", G_TYPE_INT, self->padded_height,
          "framerate", GST_TYPE_FRACTION, frn, frd,
          NULL);

      if (interlaced) {
        gst_caps_set_simple (outcaps, "interlaced", G_TYPE_BOOLEAN, TRUE, NULL);
      }

      GST_DEBUG_OBJECT (self, "outcaps: %" GST_PTR_FORMAT, outcaps);

      ret = gst_pad_set_caps (self->srcpad, outcaps);
      gst_caps_unref (outcaps);

      if (!ret) {
        GST_WARNING_OBJECT (self, "failed to set caps");
        return FALSE;
      }
    } else {
      GST_WARNING_OBJECT (self, "missing required fields");
      return FALSE;
    }
  } else {
    GST_INFO_OBJECT (self, "setcaps (src): %" GST_PTR_FORMAT, caps);
    // XXX check to make sure caps are ok.. keep track if we
    // XXX need to handle unstrided buffers..
    GST_WARNING_OBJECT (self, "TODO");
  }

  gst_object_unref (self);

  return gst_pad_set_caps (pad, caps);
}

static gboolean
gst_ducati_viddec_query (GstPad * pad, GstQuery * query)
{
  GstDucatiVidDec *self = GST_DUCATIVIDDEC (GST_OBJECT_PARENT (pad));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_BUFFERS:
      GST_DEBUG_OBJECT (self, "min buffers: %d", self->min_buffers);
      gst_query_set_buffers_count (query, self->min_buffers);

      GST_DEBUG_OBJECT (self, "min dimensions: %dx%d",
          self->padded_width, self->padded_height);
      gst_query_set_buffers_dimensions (query,
          self->padded_width, self->padded_height);
      return TRUE;
    default:
      return FALSE;
  }
}

static GstFlowReturn
gst_ducati_viddec_chain (GstPad * pad, GstBuffer * buf)
{
  GstDucatiVidDec *self = GST_DUCATIVIDDEC (GST_OBJECT_PARENT (pad));
  GstFlowReturn ret;
  Int32 err;
  GstBuffer *outbuf = NULL;

  if (G_UNLIKELY (!self->engine)) {
    GST_ERROR_OBJECT (self, "no engine");
    return GST_FLOW_ERROR;
  }

  /* do this before creating codec to ensure reverse caps negotiation
   * happens first:
   */
  ret = gst_pad_alloc_buffer_and_set_caps (self->srcpad, 0, self->outsize,
      GST_PAD_CAPS (self->srcpad), &outbuf);

  if (ret != GST_FLOW_OK) {
    outbuf = codec_bufferpool_get (self, NULL);
    ret = GST_FLOW_OK;
  }

  if (G_UNLIKELY (!self->codec)) {
    if (!codec_create (self)) {
      GST_ERROR_OBJECT (self, "could not create codec");
      return GST_FLOW_ERROR;
    }
  }

  GST_BUFFER_TIMESTAMP (outbuf) = GST_BUFFER_TIMESTAMP (buf);
  GST_BUFFER_DURATION (outbuf) = GST_BUFFER_DURATION (buf);

  /* pass new output buffer as to the decoder to decode into: */
  self->inArgs->inputID = codec_prepare_outbuf (self, outbuf);
  if (!self->inArgs->inputID) {
    GST_ERROR_OBJECT (self, "could not prepare output buffer");
    return GST_FLOW_ERROR;
  }

  self->in_size = 0;
  buf = GST_DUCATIVIDDEC_GET_CLASS (self)->push_input (self, buf);

  if (self->in_size == 0) {
    GST_DEBUG_OBJECT (self, "no input, skipping process");
    gst_buffer_unref (outbuf);
    return GST_FLOW_OK;
  }

  self->inArgs->numBytes = self->in_size;
  self->inBufs->descs[0].bufSize.bytes = self->in_size;

  if (buf) {
    // XXX
    GST_WARNING_OBJECT (self, "TODO.. can't push more than one.. need loop");
    gst_buffer_unref (buf);
    buf = NULL;
  }

  err = codec_process (self, TRUE, FALSE);
  if (err) {
    GST_ERROR_OBJECT (self, "process returned error: %d %08x",
        err, self->outArgs->extendedError);
    return GST_FLOW_ERROR;
  }

  self->first_in_buffer = FALSE;

  if (self->outArgs->outBufsInUseFlag) {
    GST_WARNING_OBJECT (self, "TODO... outBufsInUseFlag");      // XXX
  }

  return GST_FLOW_OK;
}

static gboolean
gst_ducati_viddec_event (GstPad * pad, GstEvent * event)
{
  GstDucatiVidDec *self = GST_DUCATIVIDDEC (GST_OBJECT_PARENT (pad));
  gboolean ret = TRUE;
  gboolean eos = FALSE;

  GST_INFO_OBJECT (self, "begin: event=%s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      eos = TRUE;
      /* fall-through */
    case GST_EVENT_FLUSH_STOP:
      if (!codec_flush (self, eos)) {
        GST_ERROR_OBJECT (self, "could not flush");
        return FALSE;
      }
      /* fall-through */
    default:
      ret = gst_pad_push_event (self->srcpad, event);
      break;
  }

  GST_LOG_OBJECT (self, "end");

  return ret;
}

static GstStateChangeReturn
gst_ducati_viddec_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstDucatiVidDec *self = GST_DUCATIVIDDEC (element);

  GST_INFO_OBJECT (self, "begin: changing state %s -> %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!engine_open (self)) {
        GST_ERROR_OBJECT (self, "could not open");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    goto leave;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      codec_delete (self);
      engine_close (self);
      break;
    default:
      break;
  }

leave:
  GST_LOG_OBJECT (self, "end");

  return ret;
}

/* GObject vmethod implementations */

#define VERSION_LENGTH 256

static void
gst_ducati_viddec_get_property (GObject * obj,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstDucatiVidDec *self = GST_DUCATIVIDDEC (obj);

  switch (prop_id) {
    case PROP_VERSION: {
      int err;
      char *version = gst_ducati_alloc_1d (VERSION_LENGTH);

      /* in case something fails: */
      snprintf (version, VERSION_LENGTH, "unsupported");

      if (! self->engine)
        engine_open (self);

      if (! self->codec)
        codec_create (self);

      if (self->codec) {
        self->status->data.buf = (XDAS_Int8 *) TilerMem_VirtToPhys (version);
        self->status->data.bufSize = VERSION_LENGTH;

        err = VIDDEC3_control (self->codec, XDM_GETVERSION,
            self->dynParams, self->status);
        if (err) {
          GST_ERROR_OBJECT (self, "failed XDM_GETVERSION");
        }

        self->status->data.buf = NULL;
        self->status->data.bufSize = 0;
      }

      g_value_set_string (value, version);

      MemMgr_Free (version);

      break;
    }
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
  }
}

static void
gst_ducati_viddec_finalize (GObject * obj)
{
  GstDucatiVidDec *self = GST_DUCATIVIDDEC (obj);

  codec_delete (self);
  engine_close (self);

  if (self->codec_data) {
      gst_buffer_unref (self->codec_data);
      self->codec_data = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_ducati_viddec_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
}

static void
gst_ducati_viddec_class_init (GstDucatiVidDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_get_property);
  gobject_class->finalize =
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_finalize);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_change_state);

  klass->parse_caps =
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_parse_caps);
  klass->allocate_params =
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_allocate_params);
  klass->push_input =
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_push_input);

  g_object_class_install_property (gobject_class, PROP_VERSION,
      g_param_spec_string ("version", "Version",
          "The codec version string", "",
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_ducati_viddec_init (GstDucatiVidDec * self, GstDucatiVidDecClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  self->sinkpad = gst_pad_new_from_template (
      gst_element_class_get_pad_template (gstelement_class, "sink"), "sink");
  gst_pad_set_setcaps_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_set_caps));
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_event));

  self->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_setcaps_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_ducati_viddec_set_caps));
  gst_pad_set_query_function (self->srcpad,
          GST_DEBUG_FUNCPTR (gst_ducati_viddec_query));

  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  /* sane defaults in case we need to create codec without caps negotiation
   * (for example, to get 'version' property)
   */
  self->width = 128;
  self->height = 128;
}
