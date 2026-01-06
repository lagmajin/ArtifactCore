// ReSharper disable CppMemberFunctionMayBeStatic
module;

module TimeCodeRange;


namespace ArtifactCore
{
 class TimeCodeRange::Impl{
 private:
 	
 public:
  Impl();
  ~Impl();
  TimeCode startTimeCode_;
  TimeCode stopTimeCode_;
 };
	
 TimeCodeRange::Impl::Impl()
 {

 }

 TimeCodeRange::Impl::~Impl()
 {

 }
	
 TimeCodeRange::TimeCodeRange():impl_(new Impl())
 {

 }

 TimeCodeRange::TimeCodeRange(const TimeCodeRange& other) :impl_(new Impl())
 {

 }

 TimeCodeRange::TimeCodeRange(TimeCodeRange&& other) :impl_(new Impl())
 {

 }

 TimeCodeRange::~TimeCodeRange()
 {
  delete impl_;
 }

 void TimeCodeRange::setStartTimeCode(const TimeCode& timecode)
 {

 }

 void TimeCodeRange::setStopTimeCode(const TimeCode& timecode)
 {

 }

};