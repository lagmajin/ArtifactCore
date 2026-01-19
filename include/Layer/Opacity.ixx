module;

export module Core.Opacity;

import std;
import Utils.String.UniString;

export namespace ArtifactCore {

 class Opacity
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  Opacity();
  Opacity(float value);
  Opacity(Opacity&& other) noexcept;
  ~Opacity();

  Opacity(const Opacity& other);
  Opacity& operator=(const Opacity& other);
  Opacity& operator=(Opacity&& other) noexcept;

  // Getter and setter
  float value() const;
  void setValue(float value);

  // Arithmetic operators
  Opacity operator+(const Opacity& other) const;
  Opacity operator-(const Opacity& other) const;
  Opacity operator*(float factor) const;
  Opacity operator/(float factor) const;
  Opacity& operator+=(const Opacity& other);
  Opacity& operator-=(const Opacity& other);
  Opacity& operator*=(float factor);
  Opacity& operator/=(float factor);

  // Comparison operators with epsilon
  bool operator==(const Opacity& other) const;
  bool operator!=(const Opacity& other) const;
  bool operator<(const Opacity& other) const;
  bool operator<=(const Opacity& other) const;
  bool operator>(const Opacity& other) const;
  bool operator>=(const Opacity& other) const;

  // Utility
  bool isValid() const;
  void clamp();
  UniString toString() const;
  static Opacity fromString(const UniString& str);
 };

};