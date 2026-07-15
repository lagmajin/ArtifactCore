module;

#include <algorithm>
#include <cstdint>
#include <vector>

export module Sequence.StepSequencer;

export namespace ArtifactCore {

struct StepSequencerStep {
  double value = 0.0;
  double velocity = 1.0;
  double gate = 1.0;
  bool enabled = false;
};

struct StepSequencerState {
  std::int32_t currentStep = 0;
  std::int32_t stepCount = 0;
  std::int32_t loopStart = 0;
  std::int32_t loopEnd = 0;
  bool playing = false;
};

// Transport-independent step pattern model. The host decides what a step
// means (frame, beat, sample, or note) and supplies calls to advance/trigger.
class StepSequencer {
public:
  explicit StepSequencer(std::int32_t stepCount = 16) { setStepCount(stepCount); }

  void setStepCount(std::int32_t count) {
    const auto previousCount = stepCount();
    const bool usedDefaultLoop = previousCount > 0 && loopEnd_ == previousCount - 1;
    count = std::clamp<std::int32_t>(count, 1, 4096);
    steps_.resize(static_cast<std::size_t>(count));
    loopStart_ = std::clamp(loopStart_, 0, count - 1);
    loopEnd_ = usedDefaultLoop ? count - 1 : std::clamp(loopEnd_, loopStart_, count - 1);
    currentStep_ = std::clamp(currentStep_, loopStart_, loopEnd_);
  }

  std::int32_t stepCount() const { return static_cast<std::int32_t>(steps_.size()); }

  const StepSequencerStep &step(std::int32_t index) const {
    return steps_.at(static_cast<std::size_t>(checkedIndex(index)));
  }

  void setStep(std::int32_t index, const StepSequencerStep &value) {
    steps_.at(static_cast<std::size_t>(checkedIndex(index))) = value;
  }

  std::int32_t currentStep() const { return currentStep_; }
  const StepSequencerStep &current() const { return step(currentStep_); }

  void setCurrentStep(std::int32_t index) {
    currentStep_ = std::clamp(index, loopStart_, loopEnd_);
  }

  void setLoop(std::int32_t start, std::int32_t end) {
    loopStart_ = std::clamp(start, 0, stepCount() - 1);
    loopEnd_ = std::clamp(end, loopStart_, stepCount() - 1);
    setCurrentStep(currentStep_);
  }

  void clearLoop() { setLoop(0, stepCount() - 1); }
  std::int32_t loopStart() const { return loopStart_; }
  std::int32_t loopEnd() const { return loopEnd_; }

  void start() { playing_ = true; }
  void stop() { playing_ = false; }
  bool isPlaying() const { return playing_; }

  // Advance by signed step count and return the resulting active step.
  const StepSequencerStep &advance(std::int32_t amount = 1) {
    const auto span = loopEnd_ - loopStart_ + 1;
    const auto offset = ((currentStep_ - loopStart_ + amount) % span + span) % span;
    currentStep_ = loopStart_ + offset;
    return current();
  }

  // Re-select the current step without changing the transport position.
  const StepSequencerStep &trigger() const { return current(); }

  StepSequencerState state() const {
    return {currentStep_, stepCount(), loopStart_, loopEnd_, playing_};
  }

private:
  std::int32_t checkedIndex(std::int32_t index) const {
    return std::clamp(index, 0, stepCount() - 1);
  }

  std::vector<StepSequencerStep> steps_;
  std::int32_t currentStep_ = 0;
  std::int32_t loopStart_ = 0;
  std::int32_t loopEnd_ = 0;
  bool playing_ = false;
};

}
