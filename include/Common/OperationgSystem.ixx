module;
#include <utility>

export module OperatingSystem;

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





}
