module;
#include <QString>
export module Utils.MultipleTag;

export namespace ArtifactCore {

 class MultipleTag final {
 private:
  class Impl;
  Impl* impl_;
 public:
  MultipleTag();
  ~MultipleTag();
 };

 

};