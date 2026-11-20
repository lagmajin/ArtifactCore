module;

#include <QMetaType>
#include <QPointF>
#include <QString>
#include <QVariant>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

export module Animation.KeyframePatternGenerator;

import Time.Rational;
import Math.Interpolate;

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
  struct Keyframe {
    RationalTime time;
    QVariant value;
    InterpolationType interpolation = static_cast<InterpolationType>(0);
    float cp1_x = 0.42f;
    float cp1_y = 0.0f;
    float cp2_x = 0.58f;
    float cp2_y = 1.0f;
    bool roving = false;
  };

  QVector<Keyframe> keyframes;
  QString warning;
};

struct TrajectoryKeyframe {
  RationalTime time;
  QPointF value;
};

class KeyframePatternGenerator {
public:
  static KeyframePatternResult generate(const KeyframePatternRequest &request);
  // Convert a sampled 2D motion path into deterministic keyframe candidates.
  // Invalid samples are ignored; the remaining path is linearly resampled to
  // the requested frame count.
  static QVector<TrajectoryKeyframe> generateFromTrajectory(
      const QVector<QPointF> &trajectory, int startFrame, int numFrames,
      int frameScale = 1);
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
    return QVariant(static_cast<qulonglong>(std::max(0LL, std::llround(value))));
  case QMetaType::Bool:
    return QVariant(value >= 0.5);
  case QMetaType::Float:
    return QVariant(static_cast<float>(value));
  case QMetaType::Double:
  default:
    return QVariant(value);
  }
}

inline KeyframePatternResult::Keyframe makeKeyFrame(double frame, const QVariant &value,
                             int frameScale, InterpolationType interpolation = static_cast<InterpolationType>(0)) {
  KeyframePatternResult::Keyframe keyframe;
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

inline void appendSampledCurve(QVector<KeyframePatternResult::Keyframe> &out,
                               const KeyframePatternRequest &request,
                               const QVariant &baseValue,
                               const QVariant &targetValue,
                               auto &&valueForAlpha,
                               InterpolationType interpolation = static_cast<InterpolationType>(0),
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

inline void appendRamp(QVector<KeyframePatternResult::Keyframe> &out, const KeyframePatternRequest &request,
                       const QVariant &baseValue, const QVariant &targetValue,
                       double startOffset = 0.0) {
  out.push_back(makeKeyFrame(request.startFrame + startOffset, baseValue,
                             request.frameScale, static_cast<InterpolationType>(0)));
  out.push_back(makeKeyFrame(request.endFrame + startOffset, targetValue,
                             request.frameScale, static_cast<InterpolationType>(0)));
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

inline QVector<TrajectoryKeyframe> KeyframePatternGenerator::generateFromTrajectory(
    const QVector<QPointF> &trajectory, const int startFrame, const int numFrames,
    const int frameScale) {
  QVector<QPointF> validSamples;
  validSamples.reserve(trajectory.size());
  for (const QPointF &sample : trajectory) {
    if (std::isfinite(sample.x()) && std::isfinite(sample.y())) {
      validSamples.push_back(sample);
    }
  }
  if (validSamples.isEmpty() || numFrames <= 0) return {};

  const int outputCount = std::max(1, numFrames);
  const int maxSampleIndex = static_cast<int>(validSamples.size() - 1);
  QVector<TrajectoryKeyframe> result;
  result.reserve(outputCount);
  for (int i = 0; i < outputCount; ++i) {
    const double alpha = outputCount == 1
                             ? 0.0
                             : static_cast<double>(i) /
                                   static_cast<double>(outputCount - 1);
    const double sourcePosition = alpha * static_cast<double>(validSamples.size() - 1);
    const int left = std::clamp(static_cast<int>(std::floor(sourcePosition)),
                                0, maxSampleIndex);
    const int right = std::min(left + 1, maxSampleIndex);
    const double localAlpha = sourcePosition - static_cast<double>(left);
    const QPointF value = validSamples[left] * (1.0 - localAlpha) +
                          validSamples[right] * localAlpha;
    result.push_back({RationalTime(static_cast<int64_t>(startFrame) +
                                       static_cast<int64_t>(i),
                                   static_cast<int64_t>(std::max(1, frameScale))),
                      value});
  }
  return result;
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
                                                    static_cast<InterpolationType>(4)));
    result.keyframes.push_back(detail::makeKeyFrame(start + (end - start) * 0.25, targetValue,
                                                    request.frameScale, static_cast<InterpolationType>(5)));
    result.keyframes.push_back(detail::makeKeyFrame(mid, targetValue,
                                                    request.frameScale, static_cast<InterpolationType>(5)));
    result.keyframes.push_back(detail::makeKeyFrame(end, baseValue,
                                                    request.frameScale, static_cast<InterpolationType>(3)));
    break;
  }
  case KeyframePatternPreset::Bounce: {
    detail::appendSampledCurve(
        result.keyframes, request, baseValue, targetValue,
        [&](double alpha, const QVariant &from, const QVariant &to) {
          double eased = 0.0;
          if (alpha < (1.0 / 2.75)) {
            eased = 7.5625 * alpha * alpha;
          } else if (alpha < (2.0 / 2.75)) {
            const double shifted = alpha - (1.5 / 2.75);
            eased = 7.5625 * shifted * shifted + 0.75;
          } else if (alpha < (2.5 / 2.75)) {
            const double shifted = alpha - (2.25 / 2.75);
            eased = 7.5625 * shifted * shifted + 0.9375;
          } else {
            const double shifted = alpha - (2.625 / 2.75);
            eased = 7.5625 * shifted * shifted + 0.984375;
          }
          const double startValue = detail::safeDouble(from);
          const double endValue = detail::safeDouble(to);
          return detail::numericLikeVariant(
              from, startValue + (endValue - startValue) * eased);
        },
        static_cast<InterpolationType>(0), std::max(5, request.sampleCount));
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
                                                       request.frameScale, static_cast<InterpolationType>(0)));
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
        static_cast<InterpolationType>(0), std::max(6, request.sampleCount));
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
                                                       static_cast<InterpolationType>(1)));
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
          request.frameScale, static_cast<InterpolationType>(1)));
    }
    break;
  }
  case KeyframePatternPreset::Overshoot: {
    detail::appendSampledCurve(
        result.keyframes, request, baseValue, targetValue,
        [&](double alpha, const QVariant &from, const QVariant &to) {
          constexpr double s = 1.70158;
          const double shifted = alpha - 1.0;
          const double eased = shifted * shifted * ((s + 1.0) * shifted + s) + 1.0;
          const double startValue = detail::safeDouble(from);
          const double endValue = detail::safeDouble(to);
          return detail::numericLikeVariant(
              from, startValue + (endValue - startValue) * eased);
        },
        static_cast<InterpolationType>(0), std::max(5, request.sampleCount));
    break;
  }
  case KeyframePatternPreset::Settle: {
    detail::appendSampledCurve(
        result.keyframes, request, baseValue, targetValue,
        [&](double alpha, const QVariant &from, const QVariant &to) {
          return detail::settleValue(alpha, from, to, request.amplitude,
                                     request.damping, request.settleOscillation);
        },
        static_cast<InterpolationType>(0), std::max(6, request.sampleCount));
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
                                                      static_cast<InterpolationType>(1)));
      result.keyframes.push_back(detail::makeKeyFrame(beatMid, targetValue, request.frameScale,
                                                      static_cast<InterpolationType>(4)));
      result.keyframes.push_back(detail::makeKeyFrame(std::min(beatEnd, end), baseValue,
                                                      request.frameScale, static_cast<InterpolationType>(1)));
    }
    break;
  }
  }

  if (result.keyframes.isEmpty()) {
    detail::appendRamp(result.keyframes, request, baseValue, targetValue);
  }
  return result;
}

// ---------------------------------------------------------------------------
// Beat Grid — a shared, editor-agnostic description of detected musical time.
// Phase 0 deliberately owns only detection + quantization. Playback-synchronous
// analysis and marker/snap UI are wired in later phases.
// ---------------------------------------------------------------------------

struct BeatGrid {
  double bpm = 0.0;
  double fps = 30.0;
  int frameScale = 1;
  double startFrame = 0.0;
  QVector<double> beatFrames;
  double confidence = 0.0;

  bool valid() const noexcept { return bpm > 0.0 && !beatFrames.isEmpty(); }

  double framesPerBeat() const noexcept {
    return fps > 0.0 ? 60.0 * fps / std::max(1.0e-6, bpm) : 0.0;
  }

  double nearestBeatFrame(double frame) const noexcept {
    if (beatFrames.isEmpty()) {
      return frame;
    }
    double best = beatFrames.front();
    double bestDistance = std::abs(frame - best);
    for (qsizetype i = 1; i < beatFrames.size(); ++i) {
      const double distance = std::abs(frame - beatFrames[i]);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = beatFrames[i];
      }
    }
    return best;
  }

  double quantizeFrame(double frame) const noexcept {
    return valid() ? nearestBeatFrame(frame) : frame;
  }
};

class BeatGridDetector {
public:
  struct Config {
    double minBpm = 60.0;
    double maxBpm = 180.0;
    double fps = 30.0;
    int frameScale = 1;
    double windowSeconds = 0.1;
  };

  // `envelope` holds one energy/RMS sample per 1/envelopeRate seconds.
  static BeatGrid detectFromEnergyEnvelope(const QVector<double> &envelope,
                                           double envelopeRate,
                                           const Config &config) {
    BeatGrid grid;
    grid.fps = config.fps > 0.0 ? config.fps : 30.0;
    grid.frameScale = std::max(1, config.frameScale);

    if (envelope.isEmpty() || !std::isfinite(envelopeRate) || envelopeRate <= 0.0) {
      return grid;
    }

    double sum = 0.0;
    for (const double value : envelope) {
      sum += std::isfinite(value) ? value : 0.0;
    }
    const double mean = sum / static_cast<double>(envelope.size());
    double variance = 0.0;
    for (const double value : envelope) {
      const double centered = (std::isfinite(value) ? value : 0.0) - mean;
      variance += centered * centered;
    }
    variance /= static_cast<double>(envelope.size());
    if (variance <= 1.0e-12) {
      return grid;
    }

    // Autocorrelation over the BPM search range. The smallest lag whose score
    // is within 95% of the best score is preferred to avoid octave errors.
    const double minBpm = std::max(1.0, config.minBpm);
    const double maxBpm = std::max(minBpm, config.maxBpm);
    const int minLag = std::max(1, static_cast<int>(std::floor(envelopeRate * 60.0 / maxBpm)));
    const int maxLag = std::min(static_cast<int>(envelope.size()) - 1,
                                static_cast<int>(std::ceil(envelopeRate * 60.0 / minBpm)));
    if (minLag > maxLag) {
      return grid;
    }

    double bestScore = -std::numeric_limits<double>::infinity();
    int bestLag = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag) {
      double score = 0.0;
      for (qsizetype i = lag; i < envelope.size(); ++i) {
        const double left = (std::isfinite(envelope[i]) ? envelope[i] : 0.0) - mean;
        const double right = (std::isfinite(envelope[i - lag]) ? envelope[i - lag] : 0.0) - mean;
        score += left * right;
      }
      if (score > bestScore) {
        bestScore = score;
        bestLag = lag;
      }
    }
    for (int lag = minLag; lag < bestLag; ++lag) {
      double score = 0.0;
      for (qsizetype i = lag; i < envelope.size(); ++i) {
        const double left = (std::isfinite(envelope[i]) ? envelope[i] : 0.0) - mean;
        const double right = (std::isfinite(envelope[i - lag]) ? envelope[i - lag] : 0.0) - mean;
        score += left * right;
      }
      if (score >= bestScore * 0.95) {
        bestLag = lag;
        bestScore = score;
        break;
      }
    }

    grid.bpm = 60.0 * envelopeRate / static_cast<double>(bestLag);
    while (grid.bpm < minBpm && grid.bpm > 0.0) {
      grid.bpm *= 2.0;
    }
    while (grid.bpm > maxBpm) {
      grid.bpm *= 0.5;
    }
    grid.confidence = std::clamp(bestScore / (variance * static_cast<double>(envelope.size()) + 1.0e-9),
                                 0.0, 1.0);

    const double durationFrames = static_cast<double>(envelope.size()) /
                                  envelopeRate * grid.fps;
    const double beatFrames = grid.framesPerBeat();

    // Phase: the first prominent transient keeps the grid aligned with the
    // audio instead of always starting at frame zero.
    double threshold = mean + 2.0 * std::sqrt(variance);
    qsizetype firstPeak = 0;
    for (qsizetype i = 0; i < envelope.size(); ++i) {
      const double value = std::isfinite(envelope[i]) ? envelope[i] : 0.0;
      if (value >= threshold) {
        firstPeak = i;
        break;
      }
    }
    grid.startFrame = static_cast<double>(firstPeak) / envelopeRate * grid.fps;

    for (double frame = grid.startFrame; frame <= durationFrames; frame += beatFrames) {
      grid.beatFrames.push_back(frame);
    }
    if (grid.beatFrames.isEmpty()) {
      grid.beatFrames.push_back(grid.startFrame);
    }
    return grid;
  }

  static BeatGrid detectFromPcm(const QVector<float> &mono, double sampleRate,
                                const Config &config) {
    BeatGrid grid;
    if (mono.isEmpty() || !std::isfinite(sampleRate) || sampleRate <= 0.0) {
      return grid;
    }

    const double windowSeconds = config.windowSeconds > 0.0 ? config.windowSeconds : 0.1;
    const int window = std::max(1, static_cast<int>(sampleRate * windowSeconds));
    const int hop = std::max(1, window / 2);
    QVector<double> envelope;
    envelope.reserve(static_cast<qsizetype>(mono.size() / hop) + 1);
    for (int start = 0; start < mono.size(); start += hop) {
      double sumSquares = 0.0;
      int count = 0;
      const int end = static_cast<int>(std::min<qsizetype>(
          mono.size(), static_cast<qsizetype>(start) + static_cast<qsizetype>(window)));
      for (int i = start; i < end; ++i) {
        const double sample = std::isfinite(mono[i]) ? static_cast<double>(mono[i]) : 0.0;
        sumSquares += sample * sample;
        ++count;
      }
      envelope.push_back(count > 0 ? std::sqrt(sumSquares / static_cast<double>(count)) : 0.0);
    }
    return detectFromEnergyEnvelope(envelope, sampleRate / static_cast<double>(hop), config);
  }
};

// ---------------------------------------------------------------------------
// Motion Recipe — a named combination of the existing pattern generators.
// This keeps Phase 0 free of new simulation or rendering code; the recipe only
// resolves parameters and delegates to KeyframePatternGenerator.
// ---------------------------------------------------------------------------

enum class MotionRecipeKind {
  PopIn,
  SnapBack,
  StaggerWave,
  BounceIn,
  Wiggle,
  SpringFollow,
  BeatPulse
};

struct MotionRecipe {
  MotionRecipeKind kind = MotionRecipeKind::PopIn;
  double durationFrames = 18.0;
  double amplitude = 1.0;
  double damping = 4.0;
  int selectionIndex = 0;
  int selectionCount = 1;
  quint32 seed = 1;
  bool alignToBeat = false;
};

class MotionRecipeService {
public:
  struct Result {
    KeyframePatternResult keyframes;
    QString warning;
  };

  static Result apply(const MotionRecipe &recipe, const QVariant &baseValue,
                      const QVariant &targetValue, double startFrame, double fps,
                      int frameScale, const BeatGrid *beatGrid = nullptr) {
    Result result;
    KeyframePatternRequest request;
    request.baseValue = baseValue;
    request.targetValue = targetValue;
    request.frameScale = std::max(1, frameScale);
    request.startFrame = startFrame;
    request.endFrame = startFrame + std::max(1.0, recipe.durationFrames);
    request.amplitude = recipe.amplitude;
    request.damping = recipe.damping;
    request.selectionIndex = recipe.selectionIndex;
    request.selectionCount = recipe.selectionCount;
    request.seed = recipe.seed == 0 ? 1u : recipe.seed;
    request.sampleCount = 10;
    request.bpm = fps > 0.0 ? fps * 2.0 : 120.0;

    switch (recipe.kind) {
    case MotionRecipeKind::PopIn:
      request.preset = KeyframePatternPreset::Overshoot;
      break;
    case MotionRecipeKind::SnapBack:
      request.preset = KeyframePatternPreset::Settle;
      break;
    case MotionRecipeKind::StaggerWave:
      request.preset = KeyframePatternPreset::Stagger;
      request.delayFrames = std::max(0.0, recipe.durationFrames) / 4.0;
      break;
    case MotionRecipeKind::BounceIn:
      request.preset = KeyframePatternPreset::Bounce;
      break;
    case MotionRecipeKind::Wiggle:
      request.preset = KeyframePatternPreset::Wave;
      request.cycles = 2.0;
      break;
    case MotionRecipeKind::SpringFollow:
      request.preset = KeyframePatternPreset::Settle;
      request.settleOscillation = 2.0;
      break;
    case MotionRecipeKind::BeatPulse:
      request.preset = KeyframePatternPreset::BeatSync;
      break;
    }

    if (beatGrid != nullptr && beatGrid->valid()) {
      request.bpm = beatGrid->bpm;
      if (recipe.alignToBeat) {
        const double alignedStart = beatGrid->quantizeFrame(startFrame);
        const double beatLength = beatGrid->framesPerBeat();
        const double beats = std::max(1.0, std::ceil(recipe.durationFrames / beatLength));
        request.startFrame = alignedStart;
        request.endFrame = alignedStart + beats * beatLength;
      }
    } else if (recipe.kind == MotionRecipeKind::BeatPulse) {
      result.warning = QStringLiteral(
          "BeatPulse uses an estimated BPM; provide a detected BeatGrid for musical alignment.");
    }

    result.keyframes = KeyframePatternGenerator::generate(request);
    if (result.warning.isEmpty()) {
      result.warning = result.keyframes.warning;
    }
    return result;
  }
};

} // namespace ArtifactCore
