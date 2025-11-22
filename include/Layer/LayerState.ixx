module;
#include "../Define/DllExportMacro.hpp"

export module Layer.State;


export namespace ArtifactCore {

 class LayerState {
 private:
  class Impl;
  Impl* impl_;
 public:
  LayerState();
  LayerState(const LayerState& other);
  LayerState(LayerState&& other) noexcept;
  ~LayerState();
  bool isVisible() const;
  void setVisible(bool v);

  bool isLocked() const;
  void setLocked(bool l);

  bool isSolo() const;
  void toggleSolo();
  bool isAdjustmentLayer() const;
  void setAdjustmentLayer(bool b=true);
 };







};


