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

#ifndef __GST_DUCATIVP7DEC_H__
#define __GST_DUCATIVP7DEC_H__

#include "gstducatividdec.h"

#include <ti/sdo/codecs/vp7dec/ivp7vdec.h>


G_BEGIN_DECLS

#define GST_TYPE_DUCATIVP7DEC              (gst_ducati_vp7dec_get_type())
#define GST_DUCATIVP7DEC(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DUCATIVP7DEC, GstDucatiVP7Dec))
#define GST_DUCATIVP7DEC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DUCATIVP7DEC, GstDucatiVP7DecClass))
#define GST_IS_DUCATIVP7DEC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DUCATIVP7DEC))
#define GST_IS_DUCATIVP7DEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DUCATIVP7DEC))

typedef struct _GstDucatiVP7Dec      GstDucatiVP7Dec;
typedef struct _GstDucatiVP7DecClass GstDucatiVP7DecClass;

struct _GstDucatiVP7Dec
{
  GstDucatiVidDec parent;
};

struct _GstDucatiVP7DecClass
{
  GstDucatiVidDecClass parent_class;
};

GType gst_ducati_vp7dec_get_type (void);

G_END_DECLS

#endif /* __GST_DUCATIVP7DEC_H__ */
