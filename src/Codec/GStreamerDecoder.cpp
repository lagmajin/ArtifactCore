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
  GstElement* pipeline;
  GstBus* bus;
  GstMessage* msg;
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
