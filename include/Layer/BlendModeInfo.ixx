module;
#include <array>
#include <cstddef>

#include <QString>

export module Layer.BlendModeInfo;

export import Layer.Blend;

export namespace ArtifactCore {

enum class BlendKind {
  Compositing,
  DarkenLighten,
  Contrast,
  Inversion,
  HSL,
  Stencil
};

enum class ColorContract {
  Straight,
  Premultiplied
};

struct BlendModeInfo {
  const char* uiName = "Unknown";
  const char* groupName = "Unknown";
  bool requiresHSL = false;
  bool isClassic = false;
  bool isStochastic = false;
  bool isStencil = false;
  BlendKind kind = BlendKind::Compositing;
  ColorContract srcContract = ColorContract::Straight;
  ColorContract dstContract = ColorContract::Premultiplied;
};

inline constexpr std::size_t blendModeCount =
    static_cast<std::size_t>(BlendMode::SilhouetteLuma) + 1;

inline constexpr std::array<BlendModeInfo, blendModeCount> blendModeInfoTable = {{
  {"Normal", "G0_Compositing", false, false, false, false, BlendKind::Compositing, ColorContract::Straight, ColorContract::Premultiplied},
  {"Add", "G4_Inversion", false, false, false, false, BlendKind::Inversion, ColorContract::Straight, ColorContract::Premultiplied},
  {"Subtract", "G4_Inversion", false, false, false, false, BlendKind::Inversion, ColorContract::Straight, ColorContract::Premultiplied},
  {"Multiply", "G2_DarkenLighten", false, false, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Screen", "G2_DarkenLighten", false, false, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Overlay", "G3_Contrast", false, false, false, false, BlendKind::Contrast, ColorContract::Straight, ColorContract::Premultiplied},
  {"Darken", "G2_DarkenLighten", false, false, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Lighten", "G2_DarkenLighten", false, false, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Color Dodge", "G2_DarkenLighten", false, false, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Color Burn", "G2_DarkenLighten", false, false, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Hard Light", "G3_Contrast", false, false, false, false, BlendKind::Contrast, ColorContract::Straight, ColorContract::Premultiplied},
  {"Soft Light", "G3_Contrast", false, false, false, false, BlendKind::Contrast, ColorContract::Straight, ColorContract::Premultiplied},
  {"Difference", "G4_Inversion", false, false, false, false, BlendKind::Inversion, ColorContract::Straight, ColorContract::Premultiplied},
  {"Exclusion", "G4_Inversion", false, false, false, false, BlendKind::Inversion, ColorContract::Straight, ColorContract::Premultiplied},
  {"Hue", "G5_HSL", true, false, false, false, BlendKind::HSL, ColorContract::Straight, ColorContract::Premultiplied},
  {"Saturation", "G5_HSL", true, false, false, false, BlendKind::HSL, ColorContract::Straight, ColorContract::Premultiplied},
  {"Color", "G5_HSL", true, false, false, false, BlendKind::HSL, ColorContract::Straight, ColorContract::Premultiplied},
  {"Luminosity", "G5_HSL", true, false, false, false, BlendKind::HSL, ColorContract::Straight, ColorContract::Premultiplied},
  {"Linear Burn", "G2_DarkenLighten", false, false, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Divide", "G4_Inversion", false, false, false, false, BlendKind::Inversion, ColorContract::Straight, ColorContract::Premultiplied},
  {"Pin Light", "G3_Contrast", false, false, false, false, BlendKind::Contrast, ColorContract::Straight, ColorContract::Premultiplied},
  {"Vivid Light", "G3_Contrast", false, false, false, false, BlendKind::Contrast, ColorContract::Straight, ColorContract::Premultiplied},
  {"Linear Light", "G3_Contrast", false, false, false, false, BlendKind::Contrast, ColorContract::Straight, ColorContract::Premultiplied},
  {"Hard Mix", "G3_Contrast", false, false, false, false, BlendKind::Contrast, ColorContract::Straight, ColorContract::Premultiplied},
  {"Dissolve", "G0_Compositing", false, false, true, false, BlendKind::Compositing, ColorContract::Straight, ColorContract::Premultiplied},
  {"Dancing Dissolve", "G0_Compositing", false, false, true, false, BlendKind::Compositing, ColorContract::Straight, ColorContract::Premultiplied},
  {"Classic Color Burn", "G2_DarkenLighten", false, true, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Linear Dodge", "G2_DarkenLighten", false, false, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Classic Color Dodge", "G2_DarkenLighten", false, true, false, false, BlendKind::DarkenLighten, ColorContract::Straight, ColorContract::Premultiplied},
  {"Classic Difference", "G4_Inversion", false, true, false, false, BlendKind::Inversion, ColorContract::Straight, ColorContract::Premultiplied},
  {"Stencil Alpha", "G1_Stencil", false, false, false, true, BlendKind::Stencil, ColorContract::Straight, ColorContract::Premultiplied},
  {"Stencil Luma", "G1_Stencil", false, false, false, true, BlendKind::Stencil, ColorContract::Straight, ColorContract::Premultiplied},
  {"Silhouette Alpha", "G1_Stencil", false, false, false, true, BlendKind::Stencil, ColorContract::Straight, ColorContract::Premultiplied},
  {"Silhouette Luma", "G1_Stencil", false, false, false, true, BlendKind::Stencil, ColorContract::Straight, ColorContract::Premultiplied}
}};

inline constexpr const BlendModeInfo& toInfo(BlendMode mode)
{
  const auto index = static_cast<std::size_t>(mode);
  return index < blendModeInfoTable.size() ? blendModeInfoTable[index] : blendModeInfoTable[0];
}

inline QString blendModeDisplayName(BlendMode mode)
{
  return QString::fromLatin1(toInfo(mode).uiName);
}

inline QString blendModeGroupName(BlendMode mode)
{
  return QString::fromLatin1(toInfo(mode).groupName);
}

class BlendModeCatalog {
public:
  static QString coverageReport()
  {
    QString report;
    report.reserve(static_cast<int>(blendModeInfoTable.size() * 64));
    for (std::size_t i = 0; i < blendModeInfoTable.size(); ++i) {
      const auto mode = static_cast<BlendMode>(i);
      const auto& info = toInfo(mode);
      report += QStringLiteral("%1|%2|%3|%4\n")
          .arg(blendModeDisplayName(mode))
          .arg(QString::fromLatin1(info.groupName))
          .arg(info.requiresHSL ? QStringLiteral("HSL") : QStringLiteral("-"))
          .arg(info.isStencil ? QStringLiteral("Stencil") : QStringLiteral("Core"));
    }
    return report;
  }
};

inline QString blendModeCoverageReport()
{
  return BlendModeCatalog::coverageReport();
}

} // namespace ArtifactCore
