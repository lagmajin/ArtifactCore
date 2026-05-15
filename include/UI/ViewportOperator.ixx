module;
#include <utility>
export module Operator.Viewport;

export namespace ArtifactCore {

 class ViewportOperator {
 private:
  class Impl;
  Impl* impl_;
 public:
  ViewportOperator();
  ~ViewportOperator();
  ViewportOperator(const ViewportOperator&) = delete;
  ViewportOperator& operator=(const ViewportOperator&) = delete;
 };

 class ViewportOperator::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
 };

 ViewportOperator::ViewportOperator() : impl_(new Impl())
 {
 }

 ViewportOperator::~ViewportOperator()
 {
  delete impl_;
 }

}
