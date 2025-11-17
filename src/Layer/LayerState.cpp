module;

module Layer.State;

import std;

namespace ArtifactCore {

 class LayerState::Impl {
 private:
  //bool visible_ = true;
  //float opacity_ = 1.0f;
 public:
  Impl();
  ~Impl();
  bool visible_ = true;
  bool locked_ = false;
  bool solo_ = false;
  bool guide_ = false;
  bool adjustment_ = false;
 };

 LayerState::Impl::Impl()
 {

 }

 LayerState::Impl::~Impl()
 {

 }

 LayerState::LayerState() :impl_(new Impl())
 {

 }

 LayerState::LayerState(const LayerState& other):impl_(new Impl())
 {

 }

 LayerState::LayerState(LayerState&& other) noexcept :impl_(new Impl())
 {

 }

 LayerState::~LayerState()
 {
  delete impl_;
 }

 bool LayerState::isVisible() const
 {
  return impl_->visible_;
 }

 void LayerState::setVisible(bool v)
 {
  impl_->visible_ = v;
 }

 void LayerState::setLocked(bool l)
 {

 }

 bool LayerState::isSolo() const
 {
  return impl_->solo_;
 }

 void LayerState::toggleSolo()
 {

 }


};