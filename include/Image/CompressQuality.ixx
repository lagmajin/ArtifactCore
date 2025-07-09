
module;
#include "../Define/DllExportMacro.hpp"
#include <QString>
export module CompressQuality;

export namespace ArtifactCore
{

 class LIBRARY_DLL_API CompressQuality
 {
 private:

 public:
  CompressQuality();
  ~CompressQuality();
  QString toQString() const;

 };

 CompressQuality::CompressQuality()
 {

 }

 CompressQuality::~CompressQuality()
 {

 }

 QString CompressQuality::toQString() const
 {

  return QString();
 }








};