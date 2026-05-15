module;
#include <utility>
#include <mutex>

#include <QString>

module Input.Surface.Manager;

import Event.Bus;

namespace {

using namespace ArtifactCore;

bool sameState(const InputSurfaceState& a, const InputSurfaceState& b)
{
 return a.mode == b.mode
     && a.armed == b.armed
     && a.livePreviewEnabled == b.livePreviewEnabled
     && a.stepEntryEnabled == b.stepEntryEnabled
     && a.quantizeToFrame == b.quantizeToFrame
     && a.capturePending == b.capturePending
     && a.transportFrame == b.transportFrame
     && a.stepFrame == b.stepFrame
     && a.context == b.context
     && a.targetId == b.targetId;
}

void normalizeModeFlags(InputSurfaceState& state)
{
 switch (state.mode) {
 case InputSurfaceMode::Off:
  state.armed = false;
  state.livePreviewEnabled = false;
  state.stepEntryEnabled = false;
  state.capturePending = false;
  break;
 case InputSurfaceMode::RealTimeCapture:
  state.armed = true;
  state.livePreviewEnabled = true;
  state.stepEntryEnabled = false;
  break;
 case InputSurfaceMode::StepEntry:
  state.armed = true;
  state.livePreviewEnabled = false;
  state.stepEntryEnabled = true;
  break;
 case InputSurfaceMode::Hybrid:
  state.armed = true;
  state.livePreviewEnabled = true;
  state.stepEntryEnabled = true;
  break;
 }
}

}



namespace ArtifactCore {

class InputSurfaceManager::Impl {
public:
 mutable std::mutex mutex;
 InputSurfaceState state;
};

InputSurfaceManager* InputSurfaceManager::instance()
{
 static InputSurfaceManager manager;
 return &manager;
}

InputSurfaceManager::InputSurfaceManager()
  : impl_(new Impl())
{
}

InputSurfaceManager::~InputSurfaceManager()
{
 delete impl_;
}

InputSurfaceState InputSurfaceManager::state() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state;
}

InputSurfaceMode InputSurfaceManager::mode() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.mode;
}

bool InputSurfaceManager::isArmed() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.armed;
}

bool InputSurfaceManager::isLivePreviewEnabled() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.livePreviewEnabled;
}

bool InputSurfaceManager::isStepEntryEnabled() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.stepEntryEnabled;
}

bool InputSurfaceManager::isQuantizeToFrameEnabled() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.quantizeToFrame;
}

bool InputSurfaceManager::isCapturePending() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.capturePending;
}

int64_t InputSurfaceManager::transportFrame() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.transportFrame;
}

int64_t InputSurfaceManager::stepFrame() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.stepFrame;
}

QString InputSurfaceManager::context() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.context;
}

QString InputSurfaceManager::targetId() const
{
 std::lock_guard<std::mutex> lock(impl_->mutex);
 return impl_->state.targetId;
}

void InputSurfaceManager::publishStateChanged(const InputSurfaceState& previous, const InputSurfaceState& current, const QString& reason)
{
 globalEventBus().publish(InputSurfaceStateChangedEvent{
     .previous = previous,
     .current = current,
     .reason = reason
 });
}

void InputSurfaceManager::setMode(InputSurfaceMode mode)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.mode = mode;
  normalizeModeFlags(impl_->state);
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("mode"));
 }
}

void InputSurfaceManager::setArmed(bool armed)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.armed = armed;
  if (!armed) {
   impl_->state.mode = InputSurfaceMode::Off;
   impl_->state.livePreviewEnabled = false;
   impl_->state.stepEntryEnabled = false;
   impl_->state.capturePending = false;
  }
  if (armed && impl_->state.mode == InputSurfaceMode::Off) {
   impl_->state.mode = InputSurfaceMode::RealTimeCapture;
  }
  normalizeModeFlags(impl_->state);
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("armed"));
 }
}

void InputSurfaceManager::setLivePreviewEnabled(bool enabled)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.livePreviewEnabled = enabled;
  if (enabled) {
   if (impl_->state.mode == InputSurfaceMode::Off) {
    impl_->state.mode = InputSurfaceMode::RealTimeCapture;
   } else if (impl_->state.mode == InputSurfaceMode::StepEntry) {
    impl_->state.mode = InputSurfaceMode::Hybrid;
   }
  } else if (impl_->state.mode == InputSurfaceMode::RealTimeCapture) {
   impl_->state.mode = impl_->state.stepEntryEnabled ? InputSurfaceMode::StepEntry : InputSurfaceMode::Off;
  }
  normalizeModeFlags(impl_->state);
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("livePreview"));
 }
}

void InputSurfaceManager::setStepEntryEnabled(bool enabled)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.stepEntryEnabled = enabled;
  if (enabled) {
   if (impl_->state.mode == InputSurfaceMode::Off) {
    impl_->state.mode = InputSurfaceMode::StepEntry;
   } else if (impl_->state.mode == InputSurfaceMode::RealTimeCapture) {
    impl_->state.mode = InputSurfaceMode::Hybrid;
   }
  } else if (impl_->state.mode == InputSurfaceMode::StepEntry) {
   impl_->state.mode = impl_->state.livePreviewEnabled ? InputSurfaceMode::RealTimeCapture : InputSurfaceMode::Off;
  }
  normalizeModeFlags(impl_->state);
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("stepEntry"));
 }
}

void InputSurfaceManager::setQuantizeToFrameEnabled(bool enabled)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.quantizeToFrame = enabled;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("quantize"));
 }
}

void InputSurfaceManager::setTransportFrame(int64_t frame)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.transportFrame = frame;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("transportFrame"));
 }
}

void InputSurfaceManager::setStepFrame(int64_t frame)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.stepFrame = frame;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("stepFrame"));
 }
}

void InputSurfaceManager::setContext(const QString& context)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.context = context;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("context"));
 }
}

void InputSurfaceManager::setTargetId(const QString& targetId)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.targetId = targetId;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("targetId"));
 }
}

void InputSurfaceManager::beginRealTimeCapture(const QString& targetId, const QString& context, int64_t transportFrame)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.mode = InputSurfaceMode::RealTimeCapture;
  impl_->state.armed = true;
  impl_->state.livePreviewEnabled = true;
  impl_->state.stepEntryEnabled = false;
  impl_->state.capturePending = true;
  impl_->state.transportFrame = transportFrame;
  impl_->state.targetId = targetId;
  impl_->state.context = context;
  normalizeModeFlags(impl_->state);
  impl_->state.capturePending = true;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("beginRealTimeCapture"));
 }
}

void InputSurfaceManager::beginStepEntry(const QString& targetId, const QString& context, int64_t stepFrame)
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.mode = InputSurfaceMode::StepEntry;
  impl_->state.armed = true;
  impl_->state.livePreviewEnabled = false;
  impl_->state.stepEntryEnabled = true;
  impl_->state.capturePending = true;
  impl_->state.stepFrame = stepFrame;
  impl_->state.targetId = targetId;
  impl_->state.context = context;
  normalizeModeFlags(impl_->state);
  impl_->state.capturePending = true;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("beginStepEntry"));
 }
}

void InputSurfaceManager::commitCapture()
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.capturePending = false;
  impl_->state.mode = InputSurfaceMode::Off;
  impl_->state.armed = false;
  impl_->state.livePreviewEnabled = false;
  impl_->state.stepEntryEnabled = false;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("commitCapture"));
 }
}

void InputSurfaceManager::cancelCapture()
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state.capturePending = false;
  impl_->state.mode = InputSurfaceMode::Off;
  impl_->state.armed = false;
  impl_->state.livePreviewEnabled = false;
  impl_->state.stepEntryEnabled = false;
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("cancelCapture"));
 }
}

void InputSurfaceManager::reset()
{
 InputSurfaceState previous;
 InputSurfaceState current;
 {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  previous = impl_->state;
  impl_->state = InputSurfaceState {};
  current = impl_->state;
 }
 if (!sameState(previous, current)) {
  publishStateChanged(previous, current, QStringLiteral("reset"));
 }
}

};
