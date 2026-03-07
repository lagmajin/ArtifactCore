module;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Layer.State;





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
  impl_->locked_ = l;
 }

 bool LayerState::isSolo() const
 {
  return impl_->solo_;
 }

 void LayerState::toggleSolo()
 {

 }
 
 bool LayerState::isAdjustmentLayer() const
 {
  return impl_->adjustment_;
 }

 void LayerState::setAdjustmentLayer(bool b/*=true*/)
 {
  impl_->adjustment_ = b;
 }

 bool LayerState::isLocked() const
 {
  return impl_->locked_;
 }


};