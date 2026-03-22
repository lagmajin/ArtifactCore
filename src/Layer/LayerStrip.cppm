module;
#include <cstdint>
module Layer.LayerStrip;

namespace ArtifactCore {

 class LayerStrip ::Impl {
 public:
  Impl();
  ~Impl();
 };

 LayerStrip::Impl::Impl()
 {

 }

 LayerStrip::Impl::~Impl()
 {

 }


 LayerStrip::LayerStrip():impl_(new Impl())
 {

 }

 LayerStrip::~LayerStrip()
 {
  delete impl_;
 }

 void LayerStrip::SetStartFrame(int32_t frame)
 {

 }

 void LayerStrip::SetEndFrame(int32_t frame)
 {

 }

};