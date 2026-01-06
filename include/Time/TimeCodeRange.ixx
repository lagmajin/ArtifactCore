module;
#include "../Define/DllExportMacro.hpp"
export module TimeCodeRange;

import Time.Code;

export namespace ArtifactCore
{
	
	
 class LIBRARY_DLL_API TimeCodeRange
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  TimeCodeRange();
  TimeCodeRange(const TimeCodeRange& other);
  TimeCodeRange(TimeCodeRange&& other);
  ~TimeCodeRange();
  void setStartTimeCode(const TimeCode& timecode);
  void setStopTimeCode(const TimeCode& timecode);
 };








};