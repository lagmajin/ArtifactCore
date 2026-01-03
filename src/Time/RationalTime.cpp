module;

module Time.Rational;

import std;

namespace ArtifactCore {

 class RationalTime::Impl {
 private:
  int64_t value;
  int64_t scale;
 public:
  Impl(); 
  ~Impl();
 };

 RationalTime::Impl::Impl()
 {

 }

 RationalTime::Impl::~Impl()
 {

 }

 RationalTime::RationalTime():impl_(new Impl())
 {

 }

 RationalTime::~RationalTime()
 {
  delete impl_;
  
 }

};