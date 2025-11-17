module;


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
 };







};


