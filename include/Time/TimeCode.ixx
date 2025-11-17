
module;
#include <QString>
#include "../Define/DllExportMacro.hpp"
//#include <opencv2/core/cvdef.h>
export module Time.Code;

import std;

export namespace ArtifactCore {

 class LIBRARY_DLL_API TimeCode {
 private:
  class Impl;
  Impl* impl_;

 public:
  TimeCode();
  TimeCode(int frame, double fps);
  TimeCode(int h, int m, int s, int f, double fps);
  ~TimeCode();
  TimeCode(const TimeCode&) = delete;
  TimeCode& operator=(const TimeCode&) = delete;

  TimeCode(TimeCode&&) noexcept;
  TimeCode& operator=(TimeCode&&) noexcept;

  void setByFrame(int frame);
  void setByHMSF(int h, int m, int s, int f);

  // ---- getters ----
  int frame() const;
  double  fps() const;

  void toHMSF(int& h, int& m, int& s, int& f) const;
  double toSeconds() const;

  std::string toStdString() const;
  QString toString() const;


 };



};