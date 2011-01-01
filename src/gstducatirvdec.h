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

#ifndef __GST_DUCATIRVDEC_H__
#define __GST_DUCATIRVDEC_H__

#include "gstducatividdec.h"

#include <ti/sdo/codecs/realvdec/irealvdec.h>


G_BEGIN_DECLS

#define GST_TYPE_DUCATIRVDEC              (gst_ducati_rvdec_get_type())
#define GST_DUCATIRVDEC(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DUCATIRVDEC, GstDucatiRVDec))
#define GST_DUCATIRVDEC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DUCATIRVDEC, GstDucatiRVDecClass))
#define GST_IS_DUCATIRVDEC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DUCATIRVDEC))
#define GST_IS_DUCATIRVDEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DUCATIRVDEC))

typedef struct _GstDucatiRVDec      GstDucatiRVDec;
typedef struct _GstDucatiRVDecClass GstDucatiRVDecClass;

struct _GstDucatiRVDec
{
  GstDucatiVidDec parent;

  gint rmversion;
};

struct _GstDucatiRVDecClass
{
  GstDucatiVidDecClass parent_class;
};

GType gst_ducati_rvdec_get_type (void);

G_END_DECLS

#endif /* __GST_DUCATIRVDEC_H__ */
