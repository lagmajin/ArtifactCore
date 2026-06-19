module;

#include <QString>
#include <QVariant>
#include <QColor>
#include <vector>
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
export module Property.Abstract;

import Color.Float;
import Utils.String.UniString;
import Frame.Position;
import Property.Types;
import Math.Interpolate;

import Script.Expression.Evaluator;
import Script.Expression.Value;
import Time.Rational;

export namespace ArtifactCore {

enum class PropertyType {
    Float,
    Integer,
    Boolean,
    Color,
    String,
    ObjectReference  // ID 参照
};

enum class EnvelopeMode {
    Override,
    Add,
    Multiply
};

enum class EnvelopeScope {
    Absolute,
    Entry,
    Exit
};

struct EnvelopeTrack {
    QString targetPropertyPath;
    EnvelopeScope scope = EnvelopeScope::Absolute;
    EnvelopeMode mode = EnvelopeMode::Override;
    RationalTime startTime;
    RationalTime endTime;
    double strength = 1.0;
    bool enabled = true;

    static EnvelopeTrack makeAbsolute(QString targetPropertyPath,
                                      RationalTime startTime,
                                      RationalTime endTime,
                                      EnvelopeMode mode = EnvelopeMode::Override,
                                      double strength = 1.0)
    {
        EnvelopeTrack track;
        track.targetPropertyPath = std::move(targetPropertyPath);
        track.scope = EnvelopeScope::Absolute;
        track.mode = mode;
        track.startTime = startTime;
        track.endTime = endTime;
        track.strength = strength;
        return track;
    }

    static EnvelopeTrack makeEntry(QString targetPropertyPath,
                                   RationalTime startTime,
                                   RationalTime endTime,
                                   EnvelopeMode mode = EnvelopeMode::Override,
                                   double strength = 1.0)
    {
        EnvelopeTrack track = makeAbsolute(std::move(targetPropertyPath), startTime, endTime, mode, strength);
        track.scope = EnvelopeScope::Entry;
        return track;
    }

    static EnvelopeTrack makeExit(QString targetPropertyPath,
                                  RationalTime startTime,
                                  RationalTime endTime,
                                  EnvelopeMode mode = EnvelopeMode::Override,
                                  double strength = 1.0)
    {
        EnvelopeTrack track = makeAbsolute(std::move(targetPropertyPath), startTime, endTime, mode, strength);
        track.scope = EnvelopeScope::Exit;
        return track;
    }
};

struct EnvelopePreset {
    QString name;
    QString targetPropertyPath;
    std::vector<EnvelopeTrack> tracks;

    static EnvelopePreset make(QString name,
                               QString targetPropertyPath,
                               std::vector<EnvelopeTrack> tracks)
    {
        EnvelopePreset preset;
        preset.name = std::move(name);
        preset.targetPropertyPath = std::move(targetPropertyPath);
        preset.tracks = std::move(tracks);
        return preset;
    }

    void addTrack(const EnvelopeTrack& track)
    {
        tracks.push_back(track);
    }
};

EnvelopePreset makeBlurInEnvelopePreset(QString targetPropertyPath,
                                        RationalTime startTime,
                                        RationalTime endTime,
                                        double strength = 1.0);
EnvelopePreset makeBlurOutEnvelopePreset(QString targetPropertyPath,
                                         RationalTime startTime,
                                         RationalTime endTime,
                                         double strength = 1.0);
EnvelopePreset makeSaturationOutEnvelopePreset(QString targetPropertyPath,
                                               RationalTime startTime,
                                               RationalTime endTime,
                                               double strength = 1.0);
EnvelopePreset makeGlowInEnvelopePreset(QString targetPropertyPath,
                                        RationalTime startTime,
                                        RationalTime endTime,
                                        double strength = 1.0);
EnvelopePreset makeExposureOutEnvelopePreset(QString targetPropertyPath,
                                             RationalTime startTime,
                                             RationalTime endTime,
                                             double strength = 1.0);
EnvelopePreset makeBlurInOutEnvelopePreset(QString targetPropertyPath,
                                           RationalTime inStartTime,
                                           RationalTime inEndTime,
                                           RationalTime outStartTime,
                                           RationalTime outEndTime,
                                           double inStrength = 1.0,
                                           double outStrength = 1.0);
EnvelopePreset makeGlowInOutEnvelopePreset(QString targetPropertyPath,
                                           RationalTime inStartTime,
                                           RationalTime inEndTime,
                                           RationalTime outStartTime,
                                           RationalTime outEndTime,
                                           double inStrength = 1.0,
                                           double outStrength = 1.0);
EnvelopePreset makeExposureInOutEnvelopePreset(QString targetPropertyPath,
                                               RationalTime inStartTime,
                                               RationalTime inEndTime,
                                               RationalTime outStartTime,
                                               RationalTime outEndTime,
                                               double inStrength = 1.0,
                                               double outStrength = 1.0);
EnvelopePreset makeOpacityFadeInOutEnvelopePreset(QString targetPropertyPath,
                                                  RationalTime inStartTime,
                                                  RationalTime inEndTime,
                                                  RationalTime outStartTime,
                                                  RationalTime outEndTime,
                                                  double inStrength = 1.0,
                                                  double outStrength = 1.0);

struct KeyFrame {
    enum class Anchor {
        Absolute = 0,
        LockToIn = 1,
        LockToOut = 2,
        StretchWithLayer = 3
    };

    enum class ColorLabel {
        None = 0,
        Red = 1,
        Blue = 2,
        Yellow = 3,
        Green = 4,
        Purple = 5,
        Gray = 6
    };

    RationalTime time;
    QVariant value;
    InterpolationType interpolation = InterpolationType::Linear;
    float cp1_x = 0.42f, cp1_y = 0.0f;
    float cp2_x = 0.58f, cp2_y = 1.0f;
    bool roving = false;
    Anchor anchor = Anchor::Absolute;
    ColorLabel colorLabel = ColorLabel::None;
};

struct PropertyMetadata {
    QString displayLabel;
    QString unit;
    QString tooltip;
    QVariant hardMin;
    QVariant hardMax;
    QVariant softMin;
    QVariant softMax;
    QVariant step;
    
    // 参照タイプ用
    QString referenceTypeName;  // 参照可能タイプ名（例："LayerID", "CompositionID"）
    bool allowNull = true;      // null 許可
};

class LIBRARY_DLL_API AbstractProperty {
private:
    class Impl;
    Impl* pImpl;

public:
    AbstractProperty();
    ~AbstractProperty();

    // Copy/Move - declarations only (impl in .cppm)
    AbstractProperty(const AbstractProperty& other);
    AbstractProperty& operator=(const AbstractProperty& other);
    AbstractProperty(AbstractProperty&& other) noexcept;
    AbstractProperty& operator=(AbstractProperty&& other) noexcept;

    // Getters
    QString getName() const;
    PropertyType getType() const;
    QVariant getValue() const;
    QVariant getDefaultValue() const;
    QVariant getMinValue() const;
    QVariant getMaxValue() const;
    bool isAnimatable() const;
    QColor getColorValue() const;
    PropertyMetadata metadata() const;

    // Setters
    void setName(const QString& name);
    void setType(PropertyType type);
    void setValue(const QVariant& value);
    void setDefaultValue(const QVariant& value);
    void setMinValue(const QVariant& value);
    void setMaxValue(const QVariant& value);
    void setAnimatable(bool animatable);
    void setColorValue(const QColor& color);
    void setMetadata(const PropertyMetadata& metadata);
    void setDisplayLabel(const QString& label);
    void setUnit(const QString& unit);
    void setTooltip(const QString& tooltip);
    void setStep(const QVariant& step);
    void setHardRange(const QVariant& minValue, const QVariant& maxValue);
    void setSoftRange(const QVariant& minValue, const QVariant& maxValue);

    /// @brief 表示優先度を設定する。
    /// 値が小さいほど高優先度（先頭に近い位置に表示される）。
    /// デフォルトは 0。
    /// @param priority 表示優先度（負の値も可）
    void setDisplayPriority(int priority);

    /// @brief 表示優先度を返す。
    int displayPriority() const;

    // Expression support
    void setExpression(const QString& expression);
    QString getExpression() const;
    bool hasExpression() const;
    QVariant evaluateValue(const RationalTime& time, ExpressionEvaluator* evaluator = nullptr) const;

    // Envelope support
    void addEnvelope(const EnvelopeTrack& envelope);
    void addEnvelopePreset(const EnvelopePreset& preset);
    void clearEnvelopes();
    std::vector<EnvelopeTrack> getEnvelopes() const;
    bool hasEnvelopes() const;

    // KeyFrame operations
    void addKeyFrame(const RationalTime& time, const QVariant& value);
    void addKeyFrame(const RationalTime& time, const QVariant& value, InterpolationType interpolation);
    void addKeyFrame(const RationalTime& time, const QVariant& value, InterpolationType interpolation,
                     float cp1_x, float cp1_y, float cp2_x, float cp2_y);
    void addKeyFrame(const RationalTime& time, const QVariant& value, InterpolationType interpolation,
                     float cp1_x, float cp1_y, float cp2_x, float cp2_y, bool roving);
    void setKeyFrameAnchorAt(const RationalTime& time, KeyFrame::Anchor anchor);
    KeyFrame::Anchor getKeyFrameAnchorAt(const RationalTime& time) const;
    void setKeyFrameColorLabelAt(const RationalTime& time, KeyFrame::ColorLabel label);
    KeyFrame::ColorLabel getKeyFrameColorLabelAt(const RationalTime& time) const;
    void retimeKeyFramesForLayerPointChange(const RationalTime& oldInPoint,
                                            const RationalTime& oldOutPoint,
                                            const RationalTime& newInPoint,
                                            const RationalTime& newOutPoint);
    void removeKeyFrame(const RationalTime& time);
    void clearKeyFrames();
    std::vector<KeyFrame> getKeyFrames() const;
    bool hasKeyFrameAt(const RationalTime& time) const;
    bool setKeyFrameRovingAt(const RationalTime& time, bool roving);
    bool getKeyFrameRovingAt(const RationalTime& time) const;
    QVariant interpolateValue(const RationalTime& time) const;

    // Validation
    void clampValue();
    bool isValueInRange(const QVariant& value) const;

    // External Control
    void setExternalOverride(const QVariant& value);
    void clearExternalOverride();
    bool hasExternalOverride() const;
    void setExternalNormalizedValue(double normalized);
};

};
