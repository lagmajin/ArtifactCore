#pragma once
#include <QtCore/QOperatingSystemVersion>





namespace ArtifactCore {

 enum OperatingSystem {
  Windows,
  MacOS,
  iOS,
  Android,
  Linux,
  BSD,
  Unknown
 };

 OperatingSystem getOperatingSystem() {
  QOperatingSystemVersion osVersion = QOperatingSystemVersion::current();

  // OS‚ÌŽí—Þ‚ð”»•Ê‚·‚é
  if (osVersion == QOperatingSystemVersion::Windows) {
   return Windows;
  }
  else if (osVersion == QOperatingSystemVersion::MacOS) {
   return MacOS;
  }
  else if (osVersion == QOperatingSystemVersion::iOS) {
   return iOS;
  }
  else if (osVersion == QOperatingSystemVersion::Android) {
   return Android;
  }
  else if (osVersion == QOperatingSystemVersion::Linux) {
   return Linux;
  }
  else if (osVersion == QOperatingSystemVersion::FreeBSD ||
   osVersion == QOperatingSystemVersion::NetBSD ||
   osVersion == QOperatingSystemVersion::OpenBSD) {
   return BSD;
  }
  else {
   return Unknown;
  }
 }




}