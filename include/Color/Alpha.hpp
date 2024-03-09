#pragma once




namespace Artifact {

 class AlphaPrivate;

 class Alpha {
 private:
  AlphaPrivate* const pAlpha_;
 public:
  Alpha(float alpha);
  Alpha(const Alpha& other);
  Alpha(Alpha&& other);
  ~Alpha();
  float alpha() const;
  void setAlpha(float alpha);
  PString toString() const;
  void setZero();

  void swap(Alpha& other);

  operator float() const;


  void add(float other);
  void minus(float other);
  void multiply(float other);
  void div(float other);

  Alpha& operator+(float other);

  Alpha& operator+(const Alpha& other);
  Alpha& operator-(const Alpha& other);
  Alpha& operator*(const Alpha& other);
  Alpha& operator/(const Alpha& other);


  Alpha& operator=(float alpha);
  Alpha& operator=(const Alpha& other);
  Alpha& operator=(Alpha&& other) noexcept;
 };

 bool operator==(const Alpha& alpha1, const Alpha& alpha2);
 bool operator!=(const Alpha& alpha1, const Alpha& alpha2);

 bool operator>(const Alpha& alpha1, const Alpha& alpha2);
 bool operator<(const Alpha& alpha1, const Alpha& alpha2);


 Alpha calcNewAlpha(const Alpha& alpha1, const Alpha& alpha2);










};