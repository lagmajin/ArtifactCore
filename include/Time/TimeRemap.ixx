module;

#include "../Define/DllExportMacro.hpp"

export module Time.TimeRemap;

import std;
import Frame.Rate;
import Frame.Position;

// Qt classes for compatibility
import <QObject>;
import <QVector>;
import <QPair>;

export namespace ArtifactCore {

// Time remap keyframe - maps output time to source time
struct TimeRemapKeyframe {
    double outputTime;   // Output timeline time in seconds
    double sourceTime;   // Source clip time in seconds
    
    // Interpolation type
    enum class Interpolation {
        Linear,       // Linear interpolation
        Bezier,       // Bezier curve
        Hold,         // Hold frame
        EaseIn,       // Ease in
        EaseOut,      // Ease out
        EaseInOut     // Ease in and out
    };
    
    Interpolation interpolation = Interpolation::Linear;
    
    // Bezier handles
    float bezierHandleInX = 0.0f;
    float bezierHandleInY = 0.0f;
    float bezierHandleOutX = 0.0f;
    float bezierHandleOutY = 0.0f;
};

// Frame blending mode for slow/fast motion
enum class FrameBlendMode {
    None,           // No blending, just nearest frame
    FrameMix,       // Mix between two frames
    MotionBlur,     // Motion blur simulation
    OpticalFlow     // Optical flow interpolation (future)
};

// Time remap processor
class LIBRARY_DLL_API TimeRemapProcessor {
public:
    TimeRemapProcessor();
    ~TimeRemapProcessor();
    
    // Source duration
    void setSourceDuration(double seconds);
    double sourceDuration() const { return sourceDuration_; }
    
    void setSourceFrameCount(int frames);
    int sourceFrameCount() const { return sourceFrameCount_; }
    
    void setFrameRate(const FrameRate& rate);
    const FrameRate& frameRate() const { return frameRate_; }
    
    // Keyframes - using QVector as requested
    void addKeyframe(const TimeRemapKeyframe& keyframe);
    void removeKeyframe(int index);
    void clearKeyframes();
    
    const QVector<TimeRemapKeyframe>& keyframes() const { return keyframes_; }
    void setKeyframes(const QVector<TimeRemapKeyframe>& frames) { keyframes_ = frames; }
    
    // Get mapped source time for given output time
    double mapOutputToSource(double outputTime) const;
    
    // Get output time for given source time (inverse mapping)
    double mapSourceToOutput(double sourceTime) const;
    
    // Get frame index for output time
    int getSourceFrameIndex(double outputTime) const;
    
    // Frame blending
    void setFrameBlendMode(FrameBlendMode mode);
    FrameBlendMode frameBlendMode() const { return blendMode_; }
    
    void setFrameBlendAmount(float amount);  // 0-1
    float frameBlendAmount() const { return blendAmount_; }
    
    // Speed calculation
    double getSpeedAtTime(double outputTime) const;
    
    // Convert keyframe times to frames
    void convertTimesToFrames();
    void convertFramesToTimes();
    
    // Presets
    static TimeRemapProcessor createConstantSpeed(double speed);
    static TimeRemapProcessor createRampUp(double startSpeed, double endSpeed);
    static TimeRemapProcessor createRampDown(double startSpeed, double endSpeed);
    static TimeRemapProcessor createHoldAt(double time, double holdDuration);
    static TimeRemapProcessor createReverse();
    
    // Reset
    void reset();
    
private:
    // Interpolate between keyframes
    double interpolateKeyframes(double outputTime) const;
    
    // Apply easing
    double applyEasing(double t, TimeRemapKeyframe::Interpolation interp) const;
    
    QVector<TimeRemapKeyframe> keyframes_;
    double sourceDuration_ = 10.0;  // Default 10 seconds
    int sourceFrameCount_ = 300;    // Default 30fps * 10s
    FrameRate frameRate_ = FrameRate(30, 1);
    FrameBlendMode blendMode_ = FrameBlendMode::None;
    float blendAmount_ = 0.5f;
};

// Audio time stretch processor
class LIBRARY_DLL_API AudioTimeStretchProcessor {
public:
    AudioTimeStretchProcessor();
    ~AudioTimeStretchProcessor();
    
    // Set audio parameters
    void setSampleRate(int sampleRate);
    int sampleRate() const { return sampleRate_; }
    
    void setChannels(int channels);
    int channels() const { return channels_; }
    
    // Pitch preservation
    void setPreservePitch(bool preserve);
    bool preservePitch() const { return preservePitch_; }
    
    // Process audio buffer with time stretch
    // inputSamples: input audio data
    // inputSampleCount: number of input samples
    // timeStretchRatio: 1.0 = normal, 0.5 = half speed, 2.0 = double speed
    // outputSamples: output buffer (pre-allocated)
    // Returns: number of output samples
    int processTimeStretch(
        const float* inputSamples,
        int inputSampleCount,
        double timeStretchRatio,
        float* outputSamples
    );
    
    // FFT-based time stretching (better quality)
    int processTimeStretchFFT(
        const float* inputSamples,
        int inputSampleCount,
        double timeStretchRatio,
        float* outputSamples
    );
    
    // Simple rate conversion (faster, lower quality)
    int processTimeStretchSimple(
        const float* inputSamples,
        int inputSampleCount,
        double timeStretchRatio,
        float* outputSamples
    );
    
private:
    int sampleRate_ = 48000;
    int channels_ = 2;
    bool preservePitch_ = true;
};

// Complete time remap effect for layers
class LIBRARY_DLL_API TimeRemapEffect {
public:
    TimeRemapEffect();
    ~TimeRemapEffect();
    
    // Time remap processor
    TimeRemapProcessor& remap() { return remap_; }
    const TimeRemapProcessor& remap() const { return remap_; }
    
    // Audio stretch processor
    AudioTimeStretchProcessor& audio() { return audio_; }
    const AudioTimeStretchProcessor& audio() const { return audio_; }
    
    // Enable/disable
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }
    
    // Layer has audio
    void setHasAudio(bool hasAudio);
    bool hasAudio() const { return hasAudio_; }
    
    // Process: returns source frame index to use
    // Also handles frame blending info via output parameters
    int processFrame(
        double outputTime,
        float& blendForward,    // 0-1, how much to blend with next frame
        float& blendBackward   // 0-1, how much to blend with previous frame
    );
    
    // Get time stretch ratio at current time
    double getTimeStretchRatio(double outputTime) const;
    
    // Reset
    void reset();
    
signals:
    // Qt signals for UI updates
    void keyframesChanged();
    void remapChanged();
    
private:
    TimeRemapProcessor remap_;
    AudioTimeStretchProcessor audio_;
    bool enabled_ = true;
    bool hasAudio_ = false;
};

} // namespace ArtifactCore
