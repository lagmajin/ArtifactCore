#include <gst/gst.h>


#include "../../include/Video/GStreamerDecoder.hpp"


namespace ArtifactCore {

 class GStreamerDecoderPrivate {

 public:
  GStreamerDecoderPrivate();
  ~GStreamerDecoderPrivate();

 };

 GStreamerDecoderPrivate::GStreamerDecoderPrivate()
 {
  if (!gst_is_initialized())
  {
   gst_init(nullptr, nullptr);
  }
  GstElement* pipeline=nullptr;
  GstBus* bus=nullptr;
  GstMessage* msg=nullptr;
 }

 GStreamerDecoderPrivate::~GStreamerDecoderPrivate()
 {

 }

 GStreamerDecoder::GStreamerDecoder()
 {



 }

 GStreamerDecoder::~GStreamerDecoder()
 {

 }

}
