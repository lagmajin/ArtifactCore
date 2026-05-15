module;
#include <algorithm>
#include <QColor>
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <wobjectimpl.h>

module Application.AppSettings;

import Color.Float;

namespace ArtifactCore {

W_OBJECT_IMPL(ArtifactAppSettings)

class ArtifactAppSettings::Impl {
public:
    FastSettingsStore store;
    bool safeMode = false;

    Impl() {
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(path);
        store.open(QDir(path).filePath("app_settings.cbor"));
    }
};

ArtifactAppSettings* ArtifactAppSettings::instance() {
    static ArtifactAppSettings instance;
    return &instance;
}

ArtifactAppSettings::ArtifactAppSettings() : impl_(new Impl()) {}

ArtifactAppSettings::~ArtifactAppSettings() {
    delete impl_;
}

bool ArtifactAppSettings::isSafeMode() const {
    return impl_->safeMode;
}

void ArtifactAppSettings::setSafeMode(bool enable) {
    impl_->safeMode = enable;
}

QString ArtifactAppSettings::defaultFontFamily() const {
    return impl_->store.valueString("General/DefaultFontFamily", "Segoe UI");
}

void ArtifactAppSettings::setDefaultFontFamily(const QString& family) {
    impl_->store.setValue("General/DefaultFontFamily", family);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::autoSaveIntervalMinutes() const {
    return (int)impl_->store.valueInt64("General/AutoSaveInterval", 5);
}

void ArtifactAppSettings::setAutoSaveIntervalMinutes(int minutes) {
    impl_->store.setValue("General/AutoSaveInterval", minutes);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::loadLastProjectOnStartup() const {
    return impl_->store.valueBool("General/LoadLastProject", true);
}

void ArtifactAppSettings::setLoadLastProjectOnStartup(bool enable) {
    impl_->store.setValue("General/LoadLastProject", enable);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::menuBarFontScalePercent() const {
    return (int)impl_->store.valueInt64("UI/MenuBarFontScalePercent", 132);
}

void ArtifactAppSettings::setMenuBarFontScalePercent(int percent) {
    impl_->store.setValue("UI/MenuBarFontScalePercent", std::clamp(percent, 50, 200));
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::dockTabFontPointSize() const {
    return (int)impl_->store.valueInt64("UI/DockTabFontPointSize", 16);
}

void ArtifactAppSettings::setDockTabFontPointSize(int pointSize) {
    impl_->store.setValue("UI/DockTabFontPointSize", std::clamp(pointSize, 8, 30));
    Q_EMIT settingsChanged();
}

float ArtifactAppSettings::compositionCheckerboardSize() const {
    return impl_->store.value("UI/CompositionCheckerboardSize", 16.0).toFloat();
}

void ArtifactAppSettings::setCompositionCheckerboardSize(float size) {
    impl_->store.setValue("UI/CompositionCheckerboardSize",
                          std::clamp(size, 2.0f, 128.0f));
    Q_EMIT settingsChanged();
}

Artifact::Grid::GridSettings ArtifactAppSettings::compositionGridSettings() const {
    Artifact::Grid::GridSettings settings;
    settings.majorInterval =
        impl_->store.value("UI/CompositionGrid/MajorInterval", 100.0).toFloat();
    settings.subdivisions = (int)impl_->store.valueInt64("UI/CompositionGrid/Subdivisions", 4);
    settings.showMajor = impl_->store.valueBool("UI/CompositionGrid/ShowMajor", true);
    settings.showMinor = impl_->store.valueBool("UI/CompositionGrid/ShowMinor", true);
    settings.showAxis = impl_->store.valueBool("UI/CompositionGrid/ShowAxis", true);
    settings.majorColor = ArtifactCore::FloatColor(
        impl_->store.value("UI/CompositionGrid/MajorColorR", 0.45).toFloat(),
        impl_->store.value("UI/CompositionGrid/MajorColorG", 0.45).toFloat(),
        impl_->store.value("UI/CompositionGrid/MajorColorB", 0.45).toFloat(),
        impl_->store.value("UI/CompositionGrid/MajorColorA", 0.8).toFloat());
    settings.minorColor = ArtifactCore::FloatColor(
        impl_->store.value("UI/CompositionGrid/MinorColorR", 0.25).toFloat(),
        impl_->store.value("UI/CompositionGrid/MinorColorG", 0.25).toFloat(),
        impl_->store.value("UI/CompositionGrid/MinorColorB", 0.25).toFloat(),
        impl_->store.value("UI/CompositionGrid/MinorColorA", 0.4).toFloat());
    settings.axisColor = ArtifactCore::FloatColor(
        impl_->store.value("UI/CompositionGrid/AxisColorR", 0.9).toFloat(),
        impl_->store.value("UI/CompositionGrid/AxisColorG", 0.3).toFloat(),
        impl_->store.value("UI/CompositionGrid/AxisColorB", 0.3).toFloat(),
        impl_->store.value("UI/CompositionGrid/AxisColorA", 0.9).toFloat());
    return settings;
}

void ArtifactAppSettings::setCompositionGridSettings(
    const Artifact::Grid::GridSettings& settings) {
    impl_->store.setValue("UI/CompositionGrid/MajorInterval", settings.majorInterval);
    impl_->store.setValue("UI/CompositionGrid/Subdivisions", settings.subdivisions);
    impl_->store.setValue("UI/CompositionGrid/ShowMajor", settings.showMajor);
    impl_->store.setValue("UI/CompositionGrid/ShowMinor", settings.showMinor);
    impl_->store.setValue("UI/CompositionGrid/ShowAxis", settings.showAxis);
    impl_->store.setValue("UI/CompositionGrid/MajorColorR", settings.majorColor.r());
    impl_->store.setValue("UI/CompositionGrid/MajorColorG", settings.majorColor.g());
    impl_->store.setValue("UI/CompositionGrid/MajorColorB", settings.majorColor.b());
    impl_->store.setValue("UI/CompositionGrid/MajorColorA", settings.majorColor.a());
    impl_->store.setValue("UI/CompositionGrid/MinorColorR", settings.minorColor.r());
    impl_->store.setValue("UI/CompositionGrid/MinorColorG", settings.minorColor.g());
    impl_->store.setValue("UI/CompositionGrid/MinorColorB", settings.minorColor.b());
    impl_->store.setValue("UI/CompositionGrid/MinorColorA", settings.minorColor.a());
    impl_->store.setValue("UI/CompositionGrid/AxisColorR", settings.axisColor.r());
    impl_->store.setValue("UI/CompositionGrid/AxisColorG", settings.axisColor.g());
    impl_->store.setValue("UI/CompositionGrid/AxisColorB", settings.axisColor.b());
    impl_->store.setValue("UI/CompositionGrid/AxisColorA", settings.axisColor.a());
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::compositionBackgroundMode() const {
    return (int)impl_->store.valueInt64("UI/Composition/BackgroundMode", 1);
}

void ArtifactAppSettings::setCompositionBackgroundMode(int mode) {
    impl_->store.setValue("UI/Composition/BackgroundMode", mode);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::compositionShowGrid() const {
    return impl_->store.valueBool("UI/Composition/ShowGrid", false);
}

void ArtifactAppSettings::setCompositionShowGrid(bool enable) {
    impl_->store.setValue("UI/Composition/ShowGrid", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::compositionShowGuides() const {
    return impl_->store.valueBool("UI/Composition/ShowGuides", false);
}

void ArtifactAppSettings::setCompositionShowGuides(bool enable) {
    impl_->store.setValue("UI/Composition/ShowGuides", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::compositionShowSafeMargins() const {
    return impl_->store.valueBool("UI/Composition/ShowSafeMargins", false);
}

void ArtifactAppSettings::setCompositionShowSafeMargins(bool enable) {
    impl_->store.setValue("UI/Composition/ShowSafeMargins", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::compositionShowAnchorCenterOverlay() const {
    return impl_->store.valueBool("UI/Composition/ShowAnchorCenterOverlay", false);
}

void ArtifactAppSettings::setCompositionShowAnchorCenterOverlay(bool enable) {
    impl_->store.setValue("UI/Composition/ShowAnchorCenterOverlay", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::compositionShowCameraFrustumOverlay() const {
    return impl_->store.valueBool("UI/Composition/ShowCameraFrustumOverlay", false);
}

void ArtifactAppSettings::setCompositionShowCameraFrustumOverlay(bool enable) {
    impl_->store.setValue("UI/Composition/ShowCameraFrustumOverlay", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::compositionShowMotionPathOverlay() const {
    return impl_->store.valueBool("UI/Composition/ShowMotionPathOverlay", false);
}

void ArtifactAppSettings::setCompositionShowMotionPathOverlay(bool enable) {
    impl_->store.setValue("UI/Composition/ShowMotionPathOverlay", enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::themeName() const {
    return impl_->store.valueString("UI/ThemeName", "Studio");
}

void ArtifactAppSettings::setThemeName(const QString& theme) {
    impl_->store.setValue("UI/ThemeName", theme);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::themePresetPath() const {
    return impl_->store.valueString("UI/ThemePresetPath", "");
}

void ArtifactAppSettings::setThemePresetPath(const QString& path) {
    impl_->store.setValue("UI/ThemePresetPath", path);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::renderThreadCount() const {
    return (int)impl_->store.valueInt64("Render/ThreadCount", 0); // 0 = Auto
}

void ArtifactAppSettings::setRenderThreadCount(int count) {
    impl_->store.setValue("Render/ThreadCount", count);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::toolbarShowGrid() const {
    return impl_->store.valueBool("UI/Toolbar/ShowGrid", true);
}

void ArtifactAppSettings::setToolbarShowGrid(bool enable) {
    impl_->store.setValue("UI/Toolbar/ShowGrid", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::toolbarShowGuide() const {
    return impl_->store.valueBool("UI/Toolbar/ShowGuide", true);
}

void ArtifactAppSettings::setToolbarShowGuide(bool enable) {
    impl_->store.setValue("UI/Toolbar/ShowGuide", enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::importDefaultFrameRateText() const {
    return impl_->store.valueString("Import/DefaultFrameRateText", "30 fps");
}

void ArtifactAppSettings::setImportDefaultFrameRateText(const QString& value) {
    impl_->store.setValue("Import/DefaultFrameRateText", value);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::importColorSpaceText() const {
    return impl_->store.valueString("Import/ColorSpaceText", "sRGB");
}

void ArtifactAppSettings::setImportColorSpaceText(const QString& value) {
    impl_->store.setValue("Import/ColorSpaceText", value);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::importAudioSampleRateText() const {
    return impl_->store.valueString("Import/AudioSampleRateText", "48000 Hz");
}

void ArtifactAppSettings::setImportAudioSampleRateText(const QString& value) {
    impl_->store.setValue("Import/AudioSampleRateText", value);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::importAutoDetectAlpha() const {
    return impl_->store.valueBool("Import/AutoDetectAlpha", true);
}

void ArtifactAppSettings::setImportAutoDetectAlpha(bool enable) {
    impl_->store.setValue("Import/AutoDetectAlpha", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::importInterpretFootage() const {
    return impl_->store.valueBool("Import/InterpretFootage", true);
}

void ArtifactAppSettings::setImportInterpretFootage(bool enable) {
    impl_->store.setValue("Import/InterpretFootage", enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::importFieldOrderText() const {
    return impl_->store.valueString("Import/FieldOrderText", "Progressive");
}

void ArtifactAppSettings::setImportFieldOrderText(const QString& value) {
    impl_->store.setValue("Import/FieldOrderText", value);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::importStillImageDurationSeconds() const {
    return (int)impl_->store.valueInt64("Import/StillImageDurationSeconds", 5);
}

void ArtifactAppSettings::setImportStillImageDurationSeconds(int seconds) {
    impl_->store.setValue("Import/StillImageDurationSeconds", std::clamp(seconds, 1, 3600));
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::importCreateCompositionOnImport() const {
    return impl_->store.valueBool("Import/CreateCompositionOnImport", true);
}

void ArtifactAppSettings::setImportCreateCompositionOnImport(bool enable) {
    impl_->store.setValue("Import/CreateCompositionOnImport", enable);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::projectDefaultCompositionWidth() const {
    return (int)impl_->store.valueInt64("ProjectDefaults/CompositionWidth", 1920);
}

void ArtifactAppSettings::setProjectDefaultCompositionWidth(int width) {
    impl_->store.setValue("ProjectDefaults/CompositionWidth", std::max(1, width));
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::projectDefaultCompositionHeight() const {
    return (int)impl_->store.valueInt64("ProjectDefaults/CompositionHeight", 1080);
}

void ArtifactAppSettings::setProjectDefaultCompositionHeight(int height) {
    impl_->store.setValue("ProjectDefaults/CompositionHeight", std::max(1, height));
    Q_EMIT settingsChanged();
}

double ArtifactAppSettings::projectDefaultCompositionFrameRate() const {
    return impl_->store.value("ProjectDefaults/CompositionFrameRate", 30.0).toDouble();
}

void ArtifactAppSettings::setProjectDefaultCompositionFrameRate(double fps) {
    impl_->store.setValue("ProjectDefaults/CompositionFrameRate", std::max(1.0, fps));
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::projectDefaultCompositionBackgroundColor() const {
    return impl_->store.valueString("ProjectDefaults/CompositionBackgroundColor", QStringLiteral("#ff000000"));
}

void ArtifactAppSettings::setProjectDefaultCompositionBackgroundColor(const QString& value) {
    const QColor color(value);
    impl_->store.setValue("ProjectDefaults/CompositionBackgroundColor",
                          color.isValid() ? color.name(QColor::HexArgb)
                                          : QStringLiteral("#ff000000"));
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::projectDefaultWorkspaceModeText() const {
    return impl_->store.valueString("ProjectDefaults/WorkspaceMode", "Default");
}

void ArtifactAppSettings::setProjectDefaultWorkspaceModeText(const QString& value) {
    const QString normalized = value.trimmed();
    impl_->store.setValue("ProjectDefaults/WorkspaceMode",
                          normalized.isEmpty() ? QStringLiteral("Default") : normalized);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::previewQualityText() const {
    return impl_->store.valueString("Preview/QualityText", "Adaptive");
}

void ArtifactAppSettings::setPreviewQualityText(const QString& value) {
    impl_->store.setValue("Preview/QualityText", value);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::previewResolutionPercent() const {
    return (int)impl_->store.valueInt64("Preview/ResolutionPercent", 50);
}

void ArtifactAppSettings::setPreviewResolutionPercent(int percent) {
    impl_->store.setValue("Preview/ResolutionPercent", std::clamp(percent, 25, 100));
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::previewEnableRamCache() const {
    return impl_->store.valueBool("Preview/EnableRamCache", true);
}

void ArtifactAppSettings::setPreviewEnableRamCache(bool enable) {
    impl_->store.setValue("Preview/EnableRamCache", enable);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::previewCacheSizeMB() const {
    return (int)impl_->store.valueInt64("Preview/CacheSizeMB", 4096);
}

void ArtifactAppSettings::setPreviewCacheSizeMB(int value) {
    impl_->store.setValue("Preview/CacheSizeMB", std::clamp(value, 512, 32768));
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::previewEnableDiskCache() const {
    return impl_->store.valueBool("Preview/EnableDiskCache", false);
}

void ArtifactAppSettings::setPreviewEnableDiskCache(bool enable) {
    impl_->store.setValue("Preview/EnableDiskCache", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::previewGenerateThumbnails() const {
    return impl_->store.valueBool("Preview/GenerateThumbnails", true);
}

void ArtifactAppSettings::setPreviewGenerateThumbnails(bool enable) {
    impl_->store.setValue("Preview/GenerateThumbnails", enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::previewThumbnailQualityText() const {
    return impl_->store.valueString("Preview/ThumbnailQualityText", "Medium");
}

void ArtifactAppSettings::setPreviewThumbnailQualityText(const QString& value) {
    impl_->store.setValue("Preview/ThumbnailQualityText", value);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::previewEnableGpuAcceleration() const {
    return impl_->store.valueBool("Preview/EnableGpuAcceleration", true);
}

void ArtifactAppSettings::setPreviewEnableGpuAcceleration(bool enable) {
    impl_->store.setValue("Preview/EnableGpuAcceleration", enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::previewGpuDeviceText() const {
    return impl_->store.valueString("Preview/GpuDeviceText", "Auto (Best Available)");
}

void ArtifactAppSettings::setPreviewGpuDeviceText(const QString& value) {
    impl_->store.setValue("Preview/GpuDeviceText", value);
    Q_EMIT settingsChanged();
}

void ArtifactAppSettings::sync() {
    impl_->store.sync();
}

} // namespace ArtifactCore
