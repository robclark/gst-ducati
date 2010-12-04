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

#ifndef __GST_DUCATIMPEG2DEC_H__
#define __GST_DUCATIMPEG2DEC_H__

#include <gst/gst.h>

#include "gstducatividdec.h"

#include <ti/sdo/codecs/mpeg2vdec/impeg2vdec.h>


G_BEGIN_DECLS

#define GST_TYPE_DUCATIMPEG2DEC              (gst_ducati_mpeg2dec_get_type())
#define GST_DUCATIMPEG2DEC(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DUCATIMPEG2DEC, GstDucatiMpeg2Dec))
#define GST_DUCATIMPEG2DEC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DUCATIMPEG2DEC, GstDucatiMpeg2DecClass))
#define GST_IS_DUCATIMPEG2DEC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DUCATIMPEG2DEC))
#define GST_IS_DUCATIMPEG2DEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DUCATIMPEG2DEC))

typedef struct _GstDucatiMpeg2Dec      GstDucatiMpeg2Dec;
typedef struct _GstDucatiMpeg2DecClass GstDucatiMpeg2DecClass;

struct _GstDucatiMpeg2Dec
{
  GstDucatiVidDec parent;

  gboolean prepend_codec_data;
};

struct _GstDucatiMpeg2DecClass
{
  GstDucatiVidDecClass parent_class;
};

GType gst_ducati_mpeg2dec_get_type (void);

G_END_DECLS

#endif /* __GST_DUCATIMPEG2DEC_H__ */
