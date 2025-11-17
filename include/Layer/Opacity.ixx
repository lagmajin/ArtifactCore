module;

export module Core.Opacity;

export namespace ArtifactCore {

 class Opacity
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  Opacity();
  Opacity(Opacity&& other) noexcept;
  ~Opacity();

  Opacity(const Opacity& other);
  Opacity& operator=(const Opacity& other);

  
  Opacity& operator=(Opacity&& other) noexcept;

  // 比較演算子
  bool operator==(const Opacity& other) const;
  bool operator!=(const Opacity& other) const;
  bool operator<(const Opacity& other) const;
  bool operator<=(const Opacity& other) const;
  bool operator>(const Opacity& other) const;
  bool operator>=(const Opacity& other) const;
 };










};