module;

#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include "../Define/DllExportMacro.hpp"



export module Frame.Range;



export namespace ArtifactCore {

 class LIBRARY_DLL_API FrameRange {
 private:
  class Impl;
  Impl* impl_;
 public:
  FrameRange();
  FrameRange(int start, int end);
  FrameRange(const FrameRange& other);
  FrameRange& operator=(const FrameRange& other);
  ~FrameRange();
  int start() const;
  int end() const;
  int duration() const;
 	
  void setStart(int s);
  void setEnd(int e);
  void setRange(int s, int e);
 };

 using WorkArea = FrameRange;






};