module;

#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <random>

export module Animation.KeyframePatternGenerator;

import Property.Abstract;
import Time.Rational;

export namespace ArtifactCore {

enum class KeyframePatternPreset {
  Stagger,
  Pulse,
  Bounce,
  Shake,
  Loop,
  Ramp,
  Wave,
  Step,
  RandomHold,
  Overshoot,
  Settle,
  BeatSync
};

struct KeyframePatternRequest {
  KeyframePatternPreset preset = KeyframePatternPreset::Ramp;
  QVariant baseValue;
  QVariant targetValue;
  double startFrame = 0.0;
  double endFrame = 30.0;
  double amplitude = 1.0;
  double cycles = 1.0;
  double phase = 0.0;
  double delayFrames = 2.0;
  double bpm = 120.0;
  int stepCount = 6;
  int sampleCount = 12;
  int selectionIndex = 0;
  int selectionCount = 1;
  quint32 seed = 1;
  int frameScale = 1;
  double damping = 4.0;
  double settleOscillation = 3.0;
};

struct KeyframePatternResult {
  QVector<KeyFrame> keyframes;
  QString warning;
};

class KeyframePatternGenerator {
public:
  static KeyframePatternResult generate(const KeyframePatternRequest &request);
  static QString presetLabel(KeyframePatternPreset preset);
};

namespace detail {

inline double safeDouble(const QVariant &value, bool *ok = nullptr) {
  bool converted = false;
  const double numeric = value.toDouble(&converted);
  if (ok) {
    *ok = converted;
  }
  return converted ? numeric : 0.0;
}

inline QVariant numericLikeVariant(const QVariant &templateValue, double value) {
  const int typeId = templateValue.metaType().id();
  switch (typeId) {
  case QMetaType::Int:
  case QMetaType::LongLong:
  case QMetaType::Short:
  case QMetaType::Char:
    return QVariant(static_cast<qlonglong>(std::llround(value)));
  case QMetaType::UInt:
  case QMetaType::ULongLong:
  case QMetaType::UShort:
  case QMetaType::UChar:
    return QVariant(static_cast<qulonglong>(std::max(0.0, std::llround(value))));
  case QMetaType::Bool:
    return QVariant(value >= 0.5);
  case QMetaType::Float:
    return QVariant(static_cast<float>(value));
  case QMetaType::Double:
  default:
    return QVariant(value);
  }
}

inline KeyFrame makeKeyFrame(double frame, const QVariant &value,
                             int frameScale, InterpolationType interpolation = InterpolationType::Linear) {
  KeyFrame keyframe;
  keyframe.time = RationalTime(static_cast<int64_t>(std::llround(frame)),
                               static_cast<int64_t>(std::max(1, frameScale)));
  keyframe.value = value;
  keyframe.interpolation = interpolation;
  return keyframe;
}

inline double normalizeAlpha(double alpha) {
  return std::clamp(alpha, 0.0, 1.0);
}

inline double beatFramesFor(const KeyframePatternRequest &request) {
  const double safeBpm = std::max(1.0, request.bpm);
  const double safeScale = static_cast<double>(std::max(1, request.frameScale));
  return safeScale * 60.0 / safeBpm;
}

inline QVariant numericTarget(const KeyframePatternRequest &request,
                              const QVariant &baseValue) {
  if (request.targetValue.isValid()) {
    return request.targetValue;
  }
  return numericLikeVariant(baseValue, safeDouble(baseValue) + request.amplitude);
}

inline void appendSampledCurve(QVector<KeyFrame> &out,
                               const KeyframePatternRequest &request,
                               const QVariant &baseValue,
                               const QVariant &targetValue,
                               auto &&valueForAlpha,
                               InterpolationType interpolation = InterpolationType::Linear,
                               int sampleCount = -1) {
  const int samples = std::max(2, sampleCount >= 0 ? sampleCount : request.sampleCount);
  const double start = request.startFrame;
  const double end = std::max(request.startFrame, request.endFrame);
  for (int i = 0; i < samples; ++i) {
    const double alpha = samples == 1 ? 1.0 : static_cast<double>(i) / static_cast<double>(samples - 1);
    const double frame = start + (end - start) * alpha;
    out.push_back(makeKeyFrame(frame, valueForAlpha(alpha, baseValue, targetValue),
                               request.frameScale, interpolation));
  }
}

inline void appendRamp(QVector<KeyFrame> &out, const KeyframePatternRequest &request,
                       const QVariant &baseValue, const QVariant &targetValue,
                       double startOffset = 0.0) {
  out.push_back(makeKeyFrame(request.startFrame + startOffset, baseValue,
                             request.frameScale, InterpolationType::Linear));
  out.push_back(makeKeyFrame(request.endFrame + startOffset, targetValue,
                             request.frameScale, InterpolationType::Linear));
}

inline QVariant waveValue(double alpha, const QVariant &baseValue, const QVariant &targetValue,
                          double amplitude, double cycles, double phase) {
  const double base = safeDouble(baseValue);
  const double target = safeDouble(targetValue, nullptr);
  const double center = (base + target) * 0.5;
  const double range = std::abs(target - base) * 0.5 + amplitude;
  constexpr double kTwoPi = 6.28318530717958647692;
  const double value = center + std::sin((alpha * cycles * kTwoPi) + phase) * range;
  return numericLikeVariant(baseValue, value);
}

inline QVariant settleValue(double alpha, const QVariant &baseValue, const QVariant &targetValue,
                            double amplitude, double damping, double oscillation) {
  const double base = safeDouble(baseValue);
  const double target = safeDouble(targetValue, nullptr);
  const double delta = target - base;
  constexpr double kTwoPi = 6.28318530717958647692;
  const double oscillationValue = std::exp(-std::max(0.0, damping) * alpha) *
                                  std::cos(std::max(0.0, oscillation) * kTwoPi * alpha);
  const double value = target - delta * oscillationValue + amplitude * oscillationValue;
  return numericLikeVariant(baseValue, value);
}

inline QVariant shakeValue(std::mt19937 &rng, const QVariant &baseValue, double amplitude) {
  std::uniform_real_distribution<double> dist(-1.0, 1.0);
  const double value = safeDouble(baseValue) + dist(rng) * amplitude;
  return numericLikeVariant(baseValue, value);
}

inline QVariant randomHoldValue(std::mt19937 &rng, const QVariant &baseValue, double amplitude) {
  std::uniform_real_distribution<double> dist(-1.0, 1.0);
  const double value = safeDouble(baseValue) + dist(rng) * amplitude;
  return numericLikeVariant(baseValue, value);
}

} // namespace detail

inline QString KeyframePatternGenerator::presetLabel(KeyframePatternPreset preset) {
  switch (preset) {
  case KeyframePatternPreset::Stagger: return QStringLiteral("Stagger");
  case KeyframePatternPreset::Pulse: return QStringLiteral("Pulse");
  case KeyframePatternPreset::Bounce: return QStringLiteral("Bounce");
  case KeyframePatternPreset::Shake: return QStringLiteral("Shake");
  case KeyframePatternPreset::Loop: return QStringLiteral("Loop");
  case KeyframePatternPreset::Ramp: return QStringLiteral("Ramp");
  case KeyframePatternPreset::Wave: return QStringLiteral("Wave");
  case KeyframePatternPreset::Step: return QStringLiteral("Step");
  case KeyframePatternPreset::RandomHold: return QStringLiteral("Random Hold");
  case KeyframePatternPreset::Overshoot: return QStringLiteral("Overshoot");
  case KeyframePatternPreset::Settle: return QStringLiteral("Settle");
  case KeyframePatternPreset::BeatSync: return QStringLiteral("Beat Sync");
  }
  return QStringLiteral("Pattern");
}

inline KeyframePatternResult KeyframePatternGenerator::generate(const KeyframePatternRequest &request) {
  KeyframePatternResult result;
  if (request.endFrame < request.startFrame) {
    result.warning = QStringLiteral("End frame was before start frame; values were clamped.");
  }

  bool baseOk = false;
  const double baseNumeric = detail::safeDouble(request.baseValue, &baseOk);
  if (!baseOk && request.baseValue.isValid()) {
    result.warning = QStringLiteral("Base value is not numeric; generated values default to 0.");
  }

  const QVariant baseValue = baseOk ? detail::numericLikeVariant(request.baseValue, baseNumeric)
                                    : detail::numericLikeVariant(request.baseValue, 0.0);
  const QVariant targetValue = detail::numericTarget(request, baseValue);
  const double staggerOffset = request.preset == KeyframePatternPreset::Stagger
                                   ? std::max(0.0, request.delayFrames) * static_cast<double>(std::max(0, request.selectionIndex))
                                   : 0.0;

  auto appendShiftedRamp = [&](const QVariant &fromValue, const QVariant &toValue) {
    detail::appendRamp(result.keyframes, request, fromValue, toValue, staggerOffset);
  };

  switch (request.preset) {
  case KeyframePatternPreset::Stagger: {
    appendShiftedRamp(baseValue, targetValue);
    break;
  }
  case KeyframePatternPreset::Ramp: {
    detail::appendRamp(result.keyframes, request, baseValue, targetValue);
    break;
  }
  case KeyframePatternPreset::Pulse: {
    const double start = request.startFrame;
    const double end = std::max(request.startFrame + 1.0, request.endFrame);
    const double mid = start + (end - start) * 0.5;
    result.keyframes.push_back(detail::makeKeyFrame(start, baseValue, request.frameScale,
                                                    InterpolationType::EaseOut));
    result.keyframes.push_back(detail::makeKeyFrame(start + (end - start) * 0.25, targetValue,
                                                    request.frameScale, InterpolationType::EaseInOut));
    result.keyframes.push_back(detail::makeKeyFrame(mid, targetValue,
                                                    request.frameScale, InterpolationType::EaseInOut));
    result.keyframes.push_back(detail::makeKeyFrame(end, baseValue,
                                                    request.frameScale, InterpolationType::EaseIn));
    break;
  }
  case KeyframePatternPreset::Bounce: {
    detail::appendSampledCurve(
        result.keyframes, request, baseValue, targetValue,
        [&](double alpha, const QVariant &from, const QVariant &to) {
          return detail::numericLikeVariant(
              from, ArtifactCore::interpolate(detail::safeDouble(from),
                                              detail::safeDouble(to), static_cast<float>(alpha),
                                              InterpolationType::BounceOut));
        },
        InterpolationType::Linear, std::max(5, request.sampleCount));
    break;
  }
  case KeyframePatternPreset::Shake: {
    std::mt19937 rng(request.seed == 0 ? 1u : request.seed);
    const int samples = std::max(4, request.sampleCount);
    const double start = request.startFrame;
    const double end = std::max(request.startFrame, request.endFrame);
    for (int i = 0; i < samples; ++i) {
      const double alpha = samples == 1 ? 1.0 : static_cast<double>(i) / static_cast<double>(samples - 1);
      const double frame = start + (end - start) * alpha;
      result.keyframes.push_back(detail::makeKeyFrame(frame, detail::shakeValue(rng, baseValue, request.amplitude),
                                                       request.frameScale, InterpolationType::Linear));
    }
    break;
  }
  case KeyframePatternPreset::Loop:
  case KeyframePatternPreset::Wave: {
    detail::appendSampledCurve(
        result.keyframes, request, baseValue, targetValue,
        [&](double alpha, const QVariant &from, const QVariant &to) {
          return detail::waveValue(alpha, from, to, request.amplitude,
                                   std::max(1.0, request.cycles), request.phase);
        },
        InterpolationType::Linear, std::max(6, request.sampleCount));
    break;
  }
  case KeyframePatternPreset::Step: {
    const int steps = std::max(1, request.stepCount);
    const double start = request.startFrame;
    const double end = std::max(request.startFrame, request.endFrame);
    for (int i = 0; i <= steps; ++i) {
      const double alpha = static_cast<double>(i) / static_cast<double>(steps);
      const double frame = start + (end - start) * alpha;
      const double value = detail::safeDouble(baseValue) +
                           (detail::safeDouble(targetValue) - detail::safeDouble(baseValue)) * alpha;
      result.keyframes.push_back(detail::makeKeyFrame(frame,
                                                       detail::numericLikeVariant(baseValue, value),
                                                       request.frameScale,
                                                       InterpolationType::Constant));
    }
    break;
  }
  case KeyframePatternPreset::RandomHold: {
    std::mt19937 rng(request.seed == 0 ? 1u : request.seed);
    const int steps = std::max(1, request.stepCount);
    const double start = request.startFrame;
    const double end = std::max(request.startFrame, request.endFrame);
    for (int i = 0; i <= steps; ++i) {
      const double alpha = static_cast<double>(i) / static_cast<double>(steps);
      const double frame = start + (end - start) * alpha;
      result.keyframes.push_back(detail::makeKeyFrame(
          frame, detail::randomHoldValue(rng, baseValue, request.amplitude),
          request.frameScale, InterpolationType::Constant));
    }
    break;
  }
  case KeyframePatternPreset::Overshoot: {
    detail::appendSampledCurve(
        result.keyframes, request, baseValue, targetValue,
        [&](double alpha, const QVariant &from, const QVariant &to) {
          return detail::numericLikeVariant(
              from, ArtifactCore::interpolate(detail::safeDouble(from),
                                              detail::safeDouble(to), static_cast<float>(alpha),
                                              InterpolationType::BackOut));
        },
        InterpolationType::Linear, std::max(5, request.sampleCount));
    break;
  }
  case KeyframePatternPreset::Settle: {
    detail::appendSampledCurve(
        result.keyframes, request, baseValue, targetValue,
        [&](double alpha, const QVariant &from, const QVariant &to) {
          return detail::settleValue(alpha, from, to, request.amplitude,
                                     request.damping, request.settleOscillation);
        },
        InterpolationType::Linear, std::max(6, request.sampleCount));
    break;
  }
  case KeyframePatternPreset::BeatSync: {
    const double beatFrames = std::max(1.0, detail::beatFramesFor(request));
    const double start = request.startFrame;
    const double end = std::max(request.startFrame, request.endFrame);
    const int beatCount = std::max(1, static_cast<int>(std::ceil((end - start) / beatFrames)));
    for (int i = 0; i <= beatCount; ++i) {
      const double beatStart = start + static_cast<double>(i) * beatFrames;
      const double beatMid = beatStart + beatFrames * 0.35;
      const double beatEnd = beatStart + beatFrames;
      if (beatStart > end) {
        break;
      }
      result.keyframes.push_back(detail::makeKeyFrame(beatStart, baseValue, request.frameScale,
                                                      InterpolationType::Constant));
      result.keyframes.push_back(detail::makeKeyFrame(beatMid, targetValue, request.frameScale,
                                                      InterpolationType::EaseOut));
      result.keyframes.push_back(detail::makeKeyFrame(std::min(beatEnd, end), baseValue,
                                                      request.frameScale, InterpolationType::Constant));
    }
    break;
  }
  }

  if (result.keyframes.isEmpty()) {
    detail::appendRamp(result.keyframes, request, baseValue, targetValue);
  }
  return result;
}

} // namespace ArtifactCore
