module ;

#include "../Define/DllExportMacro.hpp"
export module Preview.Quality;

export namespace ArtifactCore {

 class LIBRARY_DLL_API PreviewQuality final{
 private:
  class Impl;
  Impl* impl_;
 public:
  enum class Scale {
   Full,
   Half,
   Quarter,
   Eighth,
   Auto
  };

  enum class RenderMode {
   Normal,
   Fast,        // ìGâeOFF / ä»à’ÉJÉâÅ[èàóù
   Draft        // Ç‡Ç¡Ç∆çÌÇÈ
  };

  PreviewQuality();
  explicit PreviewQuality(Scale scale);
  PreviewQuality(const PreviewQuality& other);
  ~PreviewQuality();
  Scale quality() const;
  void setQuality(Scale scale);
 	
  bool operator==(const PreviewQuality& o) const;
  bool operator!=(const PreviewQuality& o) const;
 };

};