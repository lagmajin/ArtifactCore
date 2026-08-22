module;
#include <cstdint>

#include "../Define/DllExportMacro.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QHashFunctions>
#include <QString>
#include <wobjectdefs.h>
export module Frame.Position;

import Frame.Rate;
import Time.Rational;

export namespace ArtifactCore {

 class LIBRARY_DLL_API FramePosition final {
  private:
   class Impl;
   Impl* impl_;
  public:
   FramePosition();
   explicit FramePosition(int framePosition=0);
   FramePosition(const FramePosition& other);
   FramePosition(FramePosition&& other) noexcept;
   ~FramePosition();

   int64_t framePosition() const;

   bool isValid() const;
   bool isNegative() const;

   double toSeconds(double fps) const;
   static FramePosition fromSeconds(double seconds, double fps);

   // Rational-time conversions honoring an exact rational frame rate when
   // available. Falls back to the rounded nominal fps otherwise.
   RationalTime toRationalTime(const FrameRate& rate) const;
   static FramePosition fromRationalTime(const RationalTime& rationalTime,
                                         const FrameRate& rate);

   static FramePosition min();
   static FramePosition max();

   FramePosition& operator=(int64_t frame) noexcept;
   FramePosition& operator=(const FramePosition& other);
   FramePosition& operator=(FramePosition&& other) noexcept;

   FramePosition operator+(int64_t frames) const;
   FramePosition operator-(int64_t frames) const;
   FramePosition& operator+=(int64_t frames);
   FramePosition& operator-=(int64_t frames);

   int64_t operator-(const FramePosition& other) const;

  public: // Verdigris registration for usage in signals/slots
   // Register this type name so W_REGISTER_ARGTYPE can be applied where needed
   // (no-op here, kept to satisfy header parsing)

   // 比較
   bool operator==(const FramePosition&) const;
   bool operator!=(const FramePosition&) const;
   bool operator<(const FramePosition&) const;
   bool operator<=(const FramePosition&) const;
   bool operator>(const FramePosition&) const;
   bool operator>=(const FramePosition&) const;
  };

 inline std::size_t qHash(const FramePosition& position,
                          std::size_t seed = 0) noexcept
 {
  return qHashMulti(seed, position.framePosition());
 }

};
