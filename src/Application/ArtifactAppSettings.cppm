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

QStringList ArtifactAppSettings::recentProjectPaths() const {
    return impl_->store.value(QStringLiteral("File/RecentProjectPaths"), QStringList()).toStringList();
}

void ArtifactAppSettings::setRecentProjectPaths(const QStringList& paths) {
    impl_->store.setValue(QStringLiteral("File/RecentProjectPaths"), paths);
    Q_EMIT settingsChanged();
}

QStringList ArtifactAppSettings::recentContentsViewerSourcePaths() const {
    return impl_->store.value(QStringLiteral("ContentsViewer/RecentSourcePaths"), QStringList()).toStringList();
}

void ArtifactAppSettings::setRecentContentsViewerSourcePaths(const QStringList& paths) {
    impl_->store.setValue(QStringLiteral("ContentsViewer/RecentSourcePaths"), paths);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::lastContentsViewerSourcePath() const {
    return impl_->store.valueString(QStringLiteral("ContentsViewer/LastSourcePath"), QString());
}

void ArtifactAppSettings::setLastContentsViewerSourcePath(const QString& path) {
    impl_->store.setValue(QStringLiteral("ContentsViewer/LastSourcePath"), path);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::contentsViewerCompareWipePercent() const {
    return (int)impl_->store.valueInt64(QStringLiteral("ContentsViewer/CompareWipePercent"), 50);
}

void ArtifactAppSettings::setContentsViewerCompareWipePercent(int percent) {
    impl_->store.setValue(QStringLiteral("ContentsViewer/CompareWipePercent"), std::clamp(percent, 0, 100));
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::contentsViewerCompareSidesSwapped() const {
    return impl_->store.valueBool(QStringLiteral("ContentsViewer/CompareSidesSwapped"), false);
}

void ArtifactAppSettings::setContentsViewerCompareSidesSwapped(bool swapped) {
    impl_->store.setValue(QStringLiteral("ContentsViewer/CompareSidesSwapped"), swapped);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::contentsViewerCompareSourceAPath() const {
    return impl_->store.valueString(QStringLiteral("ContentsViewer/CompareSourceAPath"), QString());
}

void ArtifactAppSettings::setContentsViewerCompareSourceAPath(const QString& path) {
    impl_->store.setValue(QStringLiteral("ContentsViewer/CompareSourceAPath"), path);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::contentsViewerCompareSourceBPath() const {
    return impl_->store.valueString(QStringLiteral("ContentsViewer/CompareSourceBPath"), QString());
}

void ArtifactAppSettings::setContentsViewerCompareSourceBPath(const QString& path) {
    impl_->store.setValue(QStringLiteral("ContentsViewer/CompareSourceBPath"), path);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::contentsViewerAssignment() const {
    return (int)impl_->store.valueInt64(QStringLiteral("ContentsViewer/ViewerAssignment"), 1);
}

void ArtifactAppSettings::setContentsViewerAssignment(int assignment) {
    impl_->store.setValue(QStringLiteral("ContentsViewer/ViewerAssignment"), std::clamp(assignment, 1, 4));
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

bool ArtifactAppSettings::layerCacheEnabled() const {
    return impl_->store.valueBool(QStringLiteral("Render/LayerCacheEnabled"), true);
}

void ArtifactAppSettings::setLayerCacheEnabled(bool enable) {
    impl_->store.setValue(QStringLiteral("Render/LayerCacheEnabled"), enable);
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

bool ArtifactAppSettings::compositionShowDensityHeatmapOverlay() const {
    return impl_->store.valueBool("UI/Composition/ShowDensityHeatmapOverlay", false);
}

void ArtifactAppSettings::setCompositionShowDensityHeatmapOverlay(bool enable) {
    impl_->store.setValue("UI/Composition/ShowDensityHeatmapOverlay", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::compositionShowGizmoDuringDrag() const {
    return impl_->store.valueBool("UI/Composition/ShowGizmoDuringDrag", false);
}

void ArtifactAppSettings::setCompositionShowGizmoDuringDrag(bool enable) {
    impl_->store.setValue("UI/Composition/ShowGizmoDuringDrag", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::timelineAllowOverscroll() const {
    return impl_->store.valueBool("UI/Timeline/AllowOverscroll", false);
}

void ArtifactAppSettings::setTimelineAllowOverscroll(bool enable) {
    impl_->store.setValue("UI/Timeline/AllowOverscroll", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::timelineAutoKeyEnabled() const {
    return impl_->store.valueBool("UI/Timeline/AutoKeyEnabled", false);
}

void ArtifactAppSettings::setTimelineAutoKeyEnabled(bool enable) {
    impl_->store.setValue("UI/Timeline/AutoKeyEnabled", enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::timelineAutoKeyScopeText() const {
    const QString scope =
        impl_->store.valueString(QStringLiteral("UI/Timeline/AutoKeyScope"),
                                 QStringLiteral("Global"));
    const QString normalized = scope.trimmed();
    if (normalized.compare(QStringLiteral("Selected Layers"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Selected Layers");
    }
    if (normalized.compare(QStringLiteral("Current Layer"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Current Layer");
    }
    return QStringLiteral("Global");
}

void ArtifactAppSettings::setTimelineAutoKeyScopeText(const QString& value) {
    const QString normalized = value.trimmed();
    const QString scope =
        normalized.compare(QStringLiteral("Selected Layers"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("Selected Layers")
            : normalized.compare(QStringLiteral("Current Layer"), Qt::CaseInsensitive) == 0
                  ? QStringLiteral("Current Layer")
                  : QStringLiteral("Global");
    impl_->store.setValue(QStringLiteral("UI/Timeline/AutoKeyScope"), scope);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::timelineGhostingEnabled() const {
    return impl_->store.valueBool("UI/Timeline/GhostingEnabled", false);
}

void ArtifactAppSettings::setTimelineGhostingEnabled(bool enable) {
    impl_->store.setValue("UI/Timeline/GhostingEnabled", enable);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::timelineGhostingFrameCount() const {
    return static_cast<int>(impl_->store.valueInt64("UI/Timeline/GhostingFrameCount", 3));
}

void ArtifactAppSettings::setTimelineGhostingFrameCount(int count) {
    impl_->store.setValue("UI/Timeline/GhostingFrameCount", std::clamp(count, 1, 5));
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::timelineGhostingOpacity() const {
    return static_cast<int>(impl_->store.valueInt64("UI/Timeline/GhostingOpacity", 18));
}

void ArtifactAppSettings::setTimelineGhostingOpacity(int percent) {
    impl_->store.setValue("UI/Timeline/GhostingOpacity", std::clamp(percent, 4, 40));
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::timelineKeyingSetModeText() const {
    const QString mode =
        impl_->store.valueString(QStringLiteral("UI/Timeline/KeyingSetMode"),
                                 QStringLiteral("All Keyable"));
    const QString normalized = mode.trimmed();
    if (normalized.compare(QStringLiteral("Transform Only"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Transform Only");
    }
    if (normalized.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Custom");
    }
    return QStringLiteral("All Keyable");
}

void ArtifactAppSettings::setTimelineKeyingSetModeText(const QString& value) {
    const QString normalized = value.trimmed();
    const QString mode =
        normalized.compare(QStringLiteral("Transform Only"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("Transform Only")
            : normalized.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0
                  ? QStringLiteral("Custom")
                  : QStringLiteral("All Keyable");
    impl_->store.setValue(QStringLiteral("UI/Timeline/KeyingSetMode"), mode);
    Q_EMIT settingsChanged();
}

QStringList ArtifactAppSettings::timelineCustomKeyingSetPropertyPaths() const {
    return impl_->store.value(QStringLiteral("UI/Timeline/CustomKeyingSetPropertyPaths"),
                              QStringList()).toStringList();
}

void ArtifactAppSettings::setTimelineCustomKeyingSetPropertyPaths(const QStringList& paths) {
    impl_->store.setValue(QStringLiteral("UI/Timeline/CustomKeyingSetPropertyPaths"), paths);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::timelineShyActive() const {
    return impl_->store.valueBool("UI/Timeline/ShyActive", false);
}

void ArtifactAppSettings::setTimelineShyActive(bool enable) {
    impl_->store.setValue("UI/Timeline/ShyActive", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::timelineGraphEditorActive() const {
    return impl_->store.valueBool("UI/Timeline/GraphEditorActive", false);
}

void ArtifactAppSettings::setTimelineGraphEditorActive(bool enable) {
    impl_->store.setValue("UI/Timeline/GraphEditorActive", enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::timelineGraphEditorModeText() const {
    return impl_->store.valueString("UI/Timeline/GraphEditorMode", QStringLiteral("Value"));
}

void ArtifactAppSettings::setTimelineGraphEditorModeText(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    impl_->store.setValue(
        "UI/Timeline/GraphEditorMode",
        normalized == QStringLiteral("speed") ? QStringLiteral("Speed")
                                               : QStringLiteral("Value"));
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::timelineMotionBlurActive() const {
    return impl_->store.valueBool("UI/Timeline/MotionBlurActive", false);
}

void ArtifactAppSettings::setTimelineMotionBlurActive(bool enable) {
    impl_->store.setValue("UI/Timeline/MotionBlurActive", enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::timelineFrameBlendingActive() const {
    return impl_->store.valueBool("UI/Timeline/FrameBlendingActive", false);
}

void ArtifactAppSettings::setTimelineFrameBlendingActive(bool enable) {
    impl_->store.setValue("UI/Timeline/FrameBlendingActive", enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::accessibilityHandedness() const {
    const QString value = impl_->store.valueString(QStringLiteral("Accessibility/Handedness"), QStringLiteral("right"));
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("left") || normalized == QStringLiteral("right")) {
        return normalized;
    }
    return QStringLiteral("right");
}

void ArtifactAppSettings::setAccessibilityHandedness(const QString& value) {
    QString normalized = value.trimmed().toLower();
    if (normalized != QStringLiteral("left") && normalized != QStringLiteral("right")) {
        normalized = QStringLiteral("right");
    }
    impl_->store.setValue(QStringLiteral("Accessibility/Handedness"), normalized);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::accessibilityPreferLargeTargets() const {
    return impl_->store.valueBool(QStringLiteral("Accessibility/PreferLargeTargets"), false);
}

void ArtifactAppSettings::setAccessibilityPreferLargeTargets(bool enable) {
    impl_->store.setValue(QStringLiteral("Accessibility/PreferLargeTargets"), enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::accessibilityPreferHighContrastHints() const {
    return impl_->store.valueBool(QStringLiteral("Accessibility/PreferHighContrastHints"), false);
}

void ArtifactAppSettings::setAccessibilityPreferHighContrastHints(bool enable) {
    impl_->store.setValue(QStringLiteral("Accessibility/PreferHighContrastHints"), enable);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::accessibilityFontScalePercent() const {
    const qlonglong value = impl_->store.valueInt64(QStringLiteral("Accessibility/FontScalePercent"), 100);
    return static_cast<int>(std::clamp<qlonglong>(value, 100, 200));
}

void ArtifactAppSettings::setAccessibilityFontScalePercent(int percent) {
    impl_->store.setValue(QStringLiteral("Accessibility/FontScalePercent"), std::clamp(percent, 100, 200));
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::accessibilityColorDeficiencyMode() const {
    const QString mode = impl_->store.valueString(QStringLiteral("Accessibility/ColorDeficiencyMode"), QStringLiteral("none"));
    if (mode == QStringLiteral("protanopia") || mode == QStringLiteral("deuteranopia") || mode == QStringLiteral("tritanopia")) {
        return mode;
    }
    return QStringLiteral("none");
}

void ArtifactAppSettings::setAccessibilityColorDeficiencyMode(const QString& mode) {
    QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("protanopia") && normalized != QStringLiteral("deuteranopia") && normalized != QStringLiteral("tritanopia")) {
        normalized = QStringLiteral("none");
    }
    impl_->store.setValue(QStringLiteral("Accessibility/ColorDeficiencyMode"), normalized);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::accessibilityReduceHoverDependency() const {
    return impl_->store.valueBool(QStringLiteral("Accessibility/ReduceHoverDependency"), false);
}

void ArtifactAppSettings::setAccessibilityReduceHoverDependency(bool enable) {
    impl_->store.setValue(QStringLiteral("Accessibility/ReduceHoverDependency"), enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::themeName() const {
    return impl_->store.valueString("UI/ThemeName", "Maya");
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

// -- Render Farm Settings --

bool ArtifactAppSettings::farmEnabled() const {
    return impl_->store.valueBool("Render/FarmEnabled", true);
}

void ArtifactAppSettings::setFarmEnabled(bool enable) {
    impl_->store.setValue("Render/FarmEnabled", enable);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::farmWorkerCount() const {
    return (int)impl_->store.valueInt64("Render/FarmWorkerCount", 0);
}

void ArtifactAppSettings::setFarmWorkerCount(int count) {
    impl_->store.setValue("Render/FarmWorkerCount", std::max(0, count));
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::farmRetryMaxAttempts() const {
    return (int)impl_->store.valueInt64("Render/FarmRetryMaxAttempts", 3);
}

void ArtifactAppSettings::setFarmRetryMaxAttempts(int attempts) {
    impl_->store.setValue("Render/FarmRetryMaxAttempts", std::max(1, attempts));
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::farmRetryInitialBackoffMs() const {
    return (int)impl_->store.valueInt64("Render/FarmRetryInitialBackoffMs", 2000);
}

void ArtifactAppSettings::setFarmRetryInitialBackoffMs(int ms) {
    impl_->store.setValue("Render/FarmRetryInitialBackoffMs", std::max(100, ms));
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::farmRetryMaxBackoffMs() const {
    return (int)impl_->store.valueInt64("Render/FarmRetryMaxBackoffMs", 60000);
}

void ArtifactAppSettings::setFarmRetryMaxBackoffMs(int ms) {
    impl_->store.setValue("Render/FarmRetryMaxBackoffMs", std::max(static_cast<qlonglong>(ms), impl_->store.valueInt64("Render/FarmRetryInitialBackoffMs", 2000)));
    Q_EMIT settingsChanged();
}

unsigned short ArtifactAppSettings::farmRpcPort() const {
    return (unsigned short)impl_->store.valueInt64("Render/FarmRpcPort", 9876);
}

void ArtifactAppSettings::setFarmRpcPort(unsigned short port) {
    impl_->store.setValue("Render/FarmRpcPort", (int)port);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::farmAllowRemote() const {
    return impl_->store.valueBool("Render/FarmAllowRemote", false);
}

void ArtifactAppSettings::setFarmAllowRemote(bool allow) {
    impl_->store.setValue("Render/FarmAllowRemote", allow);
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

QString ArtifactAppSettings::creationDefaultsJson() const {
    return impl_->store.valueString(QStringLiteral("CreationDefaults/Json"), QString());
}

void ArtifactAppSettings::setCreationDefaultsJson(const QString& json) {
    impl_->store.setValue(QStringLiteral("CreationDefaults/Json"), json);
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
