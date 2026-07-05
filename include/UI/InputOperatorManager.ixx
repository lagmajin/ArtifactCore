module;
#include <utility>
//#include <QObject>
//#include <QString>
//#include <QVariant>
export module Input.Surface.Manager;

import Event.Bus;

import Input.Operator;

export namespace ArtifactCore
{

 enum class InputSurfaceMode {
  Off = 0,
  RealTimeCapture,
  StepEntry,
  Hybrid
 };

 struct InputSurfaceState {
  InputSurfaceMode mode = InputSurfaceMode::Off;
  bool armed = false;
  bool livePreviewEnabled = false;
  bool stepEntryEnabled = false;
  bool quantizeToFrame = true;
  bool capturePending = false;
  int64_t transportFrame = 0;
  int64_t stepFrame = 0;
  QString context;
  QString targetId;
 };

 struct InputSurfaceStateChangedEvent {
  InputSurfaceState previous;
  InputSurfaceState current;
  QString reason;
 };

 class InputSurfaceManager {
 private:
  class Impl;
  Impl* impl_;

  void publishStateChanged(const InputSurfaceState& previous,
                           const InputSurfaceState& current,
                           const QString& reason);
 public:
  static InputSurfaceManager* instance();
  InputSurfaceManager();
  ~InputSurfaceManager();

  InputSurfaceState state() const;
  InputSurfaceMode mode() const;
  bool isArmed() const;
  bool isLivePreviewEnabled() const;
  bool isStepEntryEnabled() const;
  bool isQuantizeToFrameEnabled() const;
  bool isCapturePending() const;
  int64_t transportFrame() const;
  int64_t stepFrame() const;
  QString context() const;
  QString targetId() const;

  void setMode(InputSurfaceMode mode);
  void setArmed(bool armed);
  void setLivePreviewEnabled(bool enabled);
  void setStepEntryEnabled(bool enabled);
  void setQuantizeToFrameEnabled(bool enabled);
  void setTransportFrame(int64_t frame);
  void setStepFrame(int64_t frame);
  void setContext(const QString& context);
  void setTargetId(const QString& targetId);

  void beginRealTimeCapture(const QString& targetId, const QString& context, int64_t transportFrame);
  void beginStepEntry(const QString& targetId, const QString& context, int64_t stepFrame);
  void commitCapture();
  void cancelCapture();
  void reset();
 };

}
