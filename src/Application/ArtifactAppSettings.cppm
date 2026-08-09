module;
#include <algorithm>
#include <QColor>
#include <QString>
#include <QPoint>
#include <wobjectimpl.h>

module Application.AppSettings;

import Color.Float;
import Configuration.ConfigLayer;
import Configuration.ConfigSchema;
import Configuration.LayeredConfigStore;

namespace ArtifactCore {

W_OBJECT_IMPL(ArtifactAppSettings)

namespace {
void registerBuiltInConfigSchema() {
    auto& schema = ConfigSchema::instance();
    schema.registerProperty({"General/AutoSaveInterval", "Automatic save interval in minutes", QVariant::Int, 5, 1, 120});
    schema.registerProperty({"General/DefaultFontFamily", "Default application font family", QVariant::String, QStringLiteral("Segoe UI")});
    schema.registerProperty({"General/LoadLastProject", "Load the last project on startup", QVariant::Bool, true});
    schema.registerProperty({"UI/ThemeName", "Application theme preset", QVariant::String, QStringLiteral("Maya"), {}, {},
                             {QStringLiteral("Default"), QStringLiteral("Maya"), QStringLiteral("Modo"), QStringLiteral("Studio"),
                              QStringLiteral("Blender"), QStringLiteral("DaVinci"), QStringLiteral("3ds Max"),
                              QStringLiteral("Nuke"), QStringLiteral("After Effects"), QStringLiteral("High Contrast")}, true, false});
    schema.registerProperty({"UI/MenuBarFontScalePercent", "Menu bar font scale", QVariant::Int, 132, 50, 200});
    schema.registerProperty({"UI/DockTabFontPointSize", "Dock tab font size", QVariant::Int, 16, 8, 30});
    schema.registerProperty({"Render/LayerCacheEnabled", "Enable layer cache", QVariant::Bool, true});
    schema.registerProperty({"Render/ThreadCount", "Render worker thread count", QVariant::Int, 0, 0, 256});
    schema.registerProperty({"Render/FarmEnabled", "Enable render farm", QVariant::Bool, true});
    schema.registerProperty({"Render/FarmWorkerCount", "Render farm worker count", QVariant::Int, 0, 0, 256});
    schema.registerProperty({"Render/FarmRetryMaxAttempts", "Maximum render farm retries", QVariant::Int, 3, 1, 20});
    schema.registerProperty({"Render/FarmAllowRemote", "Allow remote render farm workers", QVariant::Bool, false});
    schema.registerProperty({"Render/FarmRetryInitialBackoffMs", "Initial render farm retry backoff", QVariant::Int, 2000, 100, 60000});
    schema.registerProperty({"Render/FarmRetryMaxBackoffMs", "Maximum render farm retry backoff", QVariant::Int, 60000, 100, 300000});
    schema.registerProperty({"Render/FarmRpcPort", "Render farm RPC port", QVariant::Int, 9876, 1, 65535});
    schema.registerProperty({"UI/Toolbar/ShowGrid", "Show viewport grid", QVariant::Bool, true});
    schema.registerProperty({"UI/Toolbar/ShowGuide", "Show viewport guides", QVariant::Bool, true});
    schema.registerProperty({"UI/CompositionGrid/MajorInterval", "Composition grid major interval", QVariant::Double, 100.0, 1.0, 10000.0});
    schema.registerProperty({"UI/CompositionGrid/Subdivisions", "Composition grid subdivisions", QVariant::Int, 4, 1, 32});
    schema.registerProperty({"UI/CompositionGrid/ShowMajor", "Show major grid lines", QVariant::Bool, true});
    schema.registerProperty({"UI/CompositionGrid/ShowMinor", "Show minor grid lines", QVariant::Bool, true});
    schema.registerProperty({"UI/CompositionGrid/ShowAxis", "Show composition axes", QVariant::Bool, true});
    schema.registerProperty({"UI/Composition/BackgroundMode", "Composition background mode", QVariant::Int, 1, 0, 3});
    schema.registerProperty({"UI/Composition/ShowGrid", "Show composition grid", QVariant::Bool, false});
    schema.registerProperty({"UI/Composition/ShowGuides", "Show composition guides", QVariant::Bool, false});
    schema.registerProperty({"UI/Composition/ShowSafeMargins", "Show safe margins", QVariant::Bool, false});
    schema.registerProperty({"UI/Composition/ShowAnchorCenterOverlay", "Show anchor center overlay", QVariant::Bool, false});
    schema.registerProperty({"UI/Composition/ShowCameraFrustumOverlay", "Show camera frustum overlay", QVariant::Bool, false});
    schema.registerProperty({"UI/Composition/ShowMotionPathOverlay", "Show motion path overlay", QVariant::Bool, false});
    schema.registerProperty({"Viewport/RotationSnapDegrees", "Viewport rotation snap step", QVariant::Double, 45.0, 15.0, 90.0,
                             {15.0, 30.0, 45.0, 90.0}});
    schema.registerProperty({"AssetBrowser/StatusFilter", "Asset browser status filter", QVariant::String, QStringLiteral("all")});
    schema.registerProperty({"AssetBrowser/FileTypeFilter", "Asset browser file type filter", QVariant::String, QStringLiteral("all")});
    schema.registerProperty({"AssetBrowser/SortKey", "Asset browser sort key", QVariant::String, QStringLiteral("date")});
    schema.registerProperty({"AssetBrowser/SortAscending", "Asset browser sort direction", QVariant::Bool, false});
    schema.registerProperty({"AssetBrowser/CurrentDirectory", "Asset browser current directory", QVariant::String, QString()});
    schema.registerProperty({"AssetBrowser/ViewMode", "Asset browser view mode", QVariant::Int, 0, 0, 1});
    schema.registerProperty({"AssetBrowser/Favorites", "Asset browser favorite folders", QVariant::List, QVariantList{}});
    schema.registerProperty({"AssetBrowser/Recent", "Asset browser recent folders", QVariant::List, QVariantList{}});
    schema.registerProperty({"Viewport/AudioWaveformOverlay", "Show audio waveform overlay", QVariant::Bool, true});
    schema.registerProperty({"Viewport/AudioSpectrumOverlay", "Show audio spectrum overlay", QVariant::Bool, true});
    schema.registerProperty({"Viewport/RigOverlayVisible", "Show rig overlay", QVariant::Bool, false});
    schema.registerProperty({"Viewport/RigWeight/Radius", "Rig weight brush radius", QVariant::Double, 36.0, 2.0, 500.0});
    schema.registerProperty({"Viewport/RigWeight/Opacity", "Rig weight brush opacity", QVariant::Double, 0.35, 0.01, 1.0});
    schema.registerProperty({"Viewport/RigWeight/Flow", "Rig weight brush flow", QVariant::Double, 1.0, 0.01, 1.0});
    schema.registerProperty({"Viewport/LineDebug/RigBone", "Show rig bone debug lines", QVariant::Bool, true});
    schema.registerProperty({"Viewport/LineDebug/RigControl", "Show rig control debug lines", QVariant::Bool, true});
    schema.registerProperty({"Viewport/LineDebug/RigSkin", "Show rig skin debug lines", QVariant::Bool, true});
    schema.registerProperty({"Viewport/TrackPoint/FeatureWidth", "Tracker feature width", QVariant::Double, 24.0, 4.0, 8192.0});
    schema.registerProperty({"Viewport/TrackPoint/FeatureHeight", "Tracker feature height", QVariant::Double, 24.0, 4.0, 8192.0});
    schema.registerProperty({"Viewport/TrackPoint/SearchWidth", "Tracker search width", QVariant::Double, 96.0, 6.0, 16384.0});
    schema.registerProperty({"Viewport/TrackPoint/SearchHeight", "Tracker search height", QVariant::Double, 96.0, 6.0, 16384.0});
    schema.registerProperty({"Viewport/Hud/Visible", "Show viewport toolboxes", QVariant::Bool, true});
    schema.registerProperty({"Viewport/Hud/ToolOffset", "Viewport tool HUD offset", QVariant::Point, QPoint()});
    schema.registerProperty({"Viewport/Hud/ZoomOffset", "Viewport zoom HUD offset", QVariant::Point, QPoint()});
    schema.registerProperty({"Viewport/3DTransformClipboard", "3D transform clipboard", QVariant::List, QVariantList{}});
    for (int slot = 1; slot <= 9; ++slot) {
        schema.registerProperty({QStringLiteral("Viewport/RigPose/Slot%1").arg(slot).toStdString(),
                                 QStringLiteral("Rig pose slot %1").arg(slot).toStdString(),
                                 QVariant::Map, QVariantMap{}});
    }
    schema.registerProperty({"Viewport/MotionSketch/SampleRate", "Motion sketch sample rate", QVariant::Double, 30.0, 1.0, 240.0});
    schema.registerProperty({"Viewport/MotionSketch/Smoothing", "Motion sketch smoothing", QVariant::Double, 50.0, 0.0, 100.0});
    schema.registerProperty({"Viewport/MotionSketch/ShowWireframe", "Motion sketch wireframe", QVariant::Bool, false});
    schema.registerProperty({"Viewport/MotionSketch/ShowBackground", "Motion sketch background", QVariant::Bool, true});
    schema.registerProperty({"automation/recentCommands", "Recent command palette commands", QVariant::StringList, QStringList{}});
    schema.registerProperty({"automation/favoriteCommands", "Pinned command palette commands", QVariant::StringList, QStringList{}});
    schema.registerProperty({"automation/parameterRecipes", "Command palette parameter recipes", QVariant::ByteArray, QByteArray{}});
    schema.registerProperty({"AI/AutoInitialize", "Automatically initialize AI", QVariant::Bool, false});
    schema.registerProperty({"AI/Provider", "AI provider", QVariant::String, QStringLiteral("local")});
    schema.registerProperty({"AI/ModelPath", "AI model path", QVariant::String, QString()});
    schema.registerProperty({"AI/RecentModelPaths", "Recent AI model paths", QVariant::StringList, QStringList{}});
    schema.registerProperty({"audio/outputDeviceName", "Audio output device", QVariant::String, QString()});
    schema.registerProperty({"Asset/GenerateProxyOnImport", "Generate image proxy on import", QVariant::Bool, false});
    schema.registerProperty({"Asset/ProxyWidth", "Image proxy width", QVariant::Int, 1920, 1, 16384});
    schema.registerProperty({"Asset/ProxyHeight", "Image proxy height", QVariant::Int, 1080, 1, 16384});
    schema.registerProperty({"Asset/ProxyJpegQuality", "Image proxy JPEG quality", QVariant::Int, 85, 1, 100});
    schema.registerProperty({"UI/Composition/ShowDensityHeatmapOverlay", "Show density heatmap overlay", QVariant::Bool, false});
    schema.registerProperty({"UI/Composition/ShowGizmoDuringDrag", "Show gizmo while dragging", QVariant::Bool, false});
    schema.registerProperty({"UI/Timeline/AutoKeyEnabled", "Enable timeline auto-key", QVariant::Bool, false});
    schema.registerProperty({"UI/Timeline/AutoKeyScope", "Timeline auto-key scope", QVariant::String, QStringLiteral("All Keyable"), {}, {},
                             {QStringLiteral("Global"), QStringLiteral("Selected Layers"), QStringLiteral("Current Layer"), QStringLiteral("All Keyable")}});
    schema.registerProperty({"UI/Timeline/GhostingEnabled", "Enable timeline ghosting", QVariant::Bool, false});
    schema.registerProperty({"UI/Timeline/GhostingFrameCount", "Timeline ghosting frame count", QVariant::Int, 3, 1, 20});
    schema.registerProperty({"UI/Timeline/GhostingOpacity", "Timeline ghosting opacity", QVariant::Int, 18, 4, 40});
    schema.registerProperty({"UI/Timeline/GraphEditorActive", "Enable graph editor", QVariant::Bool, false});
    schema.registerProperty({"UI/Timeline/GraphEditorMode", "Graph editor mode", QVariant::String, QStringLiteral("Value"), {}, {},
                             {QStringLiteral("Value"), QStringLiteral("Speed")}});
    schema.registerProperty({"UI/Timeline/MotionBlurActive", "Enable timeline motion blur", QVariant::Bool, false});
    schema.registerProperty({"UI/Timeline/MotionBlurShutterAngle", "Motion blur shutter angle", QVariant::Double, 180.0, 0.0, 720.0});
    schema.registerProperty({"UI/Timeline/MotionBlurSampleCount", "Motion blur sample count", QVariant::Int, 8, 1, 32});
    schema.registerProperty({"UI/Timeline/AllowOverscroll", "Allow timeline overscroll", QVariant::Bool, false});
    schema.registerProperty({"UI/Timeline/ShyActive", "Enable timeline shy mode", QVariant::Bool, false});
    schema.registerProperty({"UI/Timeline/FrameBlendingActive", "Enable timeline frame blending", QVariant::Bool, false});
    schema.registerProperty({"Accessibility/Handedness", "Interface handedness", QVariant::String, QStringLiteral("right"), {}, {},
                             {QStringLiteral("left"), QStringLiteral("right")}});
    schema.registerProperty({"Accessibility/FontScalePercent", "Accessibility font scale", QVariant::Int, 100, 100, 200});
    schema.registerProperty({"Accessibility/ColorDeficiencyMode", "Color deficiency simulation mode", QVariant::String, QStringLiteral("none"), {}, {},
                             {QStringLiteral("none"), QStringLiteral("protanopia"), QStringLiteral("deuteranopia"), QStringLiteral("tritanopia")}});
    schema.registerProperty({"Import/DefaultFrameRateText", "Default import frame rate", QVariant::String, QStringLiteral("30 fps")});
    schema.registerProperty({"Import/ColorSpaceText", "Default import color space", QVariant::String, QStringLiteral("sRGB")});
    schema.registerProperty({"Import/AudioSampleRateText", "Default import audio sample rate", QVariant::String, QStringLiteral("48000 Hz")});
    schema.registerProperty({"Import/AutoDetectAlpha", "Detect alpha during import", QVariant::Bool, true});
    schema.registerProperty({"Import/InterpretFootage", "Interpret footage during import", QVariant::Bool, true});
    schema.registerProperty({"Import/StillImageDurationSeconds", "Still image duration", QVariant::Int, 5, 1, 3600});
    schema.registerProperty({"Import/CreateCompositionOnImport", "Create composition on import", QVariant::Bool, true});
    schema.registerProperty({"Preview/QualityText", "Preview quality", QVariant::String, QStringLiteral("Adaptive")});
    schema.registerProperty({"Preview/ResolutionPercent", "Preview resolution", QVariant::Int, 50, 25, 100});
    schema.registerProperty({"Preview/EnableRamCache", "Enable RAM preview cache", QVariant::Bool, true});
    schema.registerProperty({"Preview/CacheSizeMB", "Preview cache size", QVariant::Int, 4096, 512, 32768});
    schema.registerProperty({"Preview/EnableDiskCache", "Enable disk preview cache", QVariant::Bool, false});
    schema.registerProperty({"Preview/GenerateThumbnails", "Generate preview thumbnails", QVariant::Bool, true});
    schema.registerProperty({"Preview/EnableGpuAcceleration", "Enable GPU preview acceleration", QVariant::Bool, true});
    schema.registerProperty({"Preview/GpuDeviceText", "Preview GPU device", QVariant::String, QStringLiteral("Auto (Best Available)")});
    schema.registerProperty({"Preview/ThumbnailQualityText", "Preview thumbnail quality", QVariant::String, QStringLiteral("Medium")});
    schema.registerProperty({"Import/FieldOrderText", "Default import field order", QVariant::String, QStringLiteral("Progressive")});
    schema.registerProperty({"UI/ThemePresetPath", "Custom theme preset path", QVariant::String, QString()});
    schema.registerProperty({"UI/Timeline/KeyingSetMode", "Timeline keying set mode", QVariant::String, QStringLiteral("All Keyable"), {}, {},
                             {QStringLiteral("All Keyable"), QStringLiteral("Transform Only"), QStringLiteral("Custom")}});
    schema.registerProperty({"Accessibility/PreferLargeTargets", "Prefer larger UI targets", QVariant::Bool, false});
    schema.registerProperty({"Accessibility/PreferHighContrastHints", "Prefer high contrast hints", QVariant::Bool, false});
    schema.registerProperty({"Accessibility/ReduceHoverDependency", "Reduce hover-only interactions", QVariant::Bool, false});
    schema.registerProperty({"Accessibility/StickyKeysEnabled", "Enable sticky modifier keys", QVariant::Bool, false});
    schema.registerProperty({"Accessibility/StickyKeysMode", "Sticky modifier key mode", QVariant::String, QStringLiteral("latch"), {}, {},
                             {QStringLiteral("latch"), QStringLiteral("lock"), QStringLiteral("both")}});
    schema.registerProperty({"Accessibility/SingleHandModeEnabled", "Enable single-hand modifier bindings", QVariant::Bool, false});
    schema.registerProperty({"Accessibility/ViewportMagnifierEnabled", "Enable viewport magnifier", QVariant::Bool, false});
    schema.registerProperty({"Accessibility/ViewportMagnifierScale", "Viewport magnifier scale", QVariant::Int, 2, 2, 8});
    schema.registerProperty({"File/RecentProjectPaths", "Recent project paths", QVariant::StringList, QStringList()});
    schema.registerProperty({"ContentsViewer/RecentSourcePaths", "Recent contents viewer sources", QVariant::StringList, QStringList()});
    schema.registerProperty({"ContentsViewer/LastSourcePath", "Last contents viewer source", QVariant::String, QString()});
    schema.registerProperty({"ContentsViewer/CompareWipePercent", "Contents viewer compare wipe", QVariant::Int, 50, 0, 100});
    schema.registerProperty({"ContentsViewer/CompareSidesSwapped", "Swap contents viewer compare sides", QVariant::Bool, false});
    schema.registerProperty({"ContentsViewer/CompareSourceAPath", "Contents viewer compare source A", QVariant::String, QString()});
    schema.registerProperty({"ContentsViewer/CompareSourceBPath", "Contents viewer compare source B", QVariant::String, QString()});
    schema.registerProperty({"ContentsViewer/ViewerAssignment", "Contents viewer assignment", QVariant::Int, 1, 1, 4});
    schema.registerProperty({"CreationDefaults/Json", "Layer creation defaults", QVariant::String, QString()});
    schema.registerProperty({"ProjectDefaults/CompositionWidth", "Default composition width", QVariant::Int, 1920, 1, 16384});
    schema.registerProperty({"ProjectDefaults/CompositionHeight", "Default composition height", QVariant::Int, 1080, 1, 16384});
    schema.registerProperty({"ProjectDefaults/CompositionBackgroundColor", "Default composition background color", QVariant::String, QStringLiteral("#ff000000")});
    schema.registerProperty({"ProjectDefaults/WorkspaceMode", "Default project workspace mode", QVariant::String, QStringLiteral("Default")});
    schema.registerProperty({"ProjectDefaults/CompositionFrameRate", "Default composition frame rate", QVariant::Double, 30.0, 1.0, 240.0});
    schema.registerProperty({"UI/CompositionCheckerboardSize", "Composition checkerboard size", QVariant::Double, 16.0, 1.0, 256.0});
    schema.registerProperty({"UI/Timeline/CustomKeyingSetPropertyPaths", "Custom timeline keying paths", QVariant::StringList, QStringList()});
    schema.registerProperty({"UI/CompositionGrid/MajorColorR", "Major grid red channel", QVariant::Double, 0.45, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/MajorColorG", "Major grid green channel", QVariant::Double, 0.45, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/MajorColorB", "Major grid blue channel", QVariant::Double, 0.45, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/MajorColorA", "Major grid alpha channel", QVariant::Double, 0.8, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/MinorColorR", "Minor grid red channel", QVariant::Double, 0.25, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/MinorColorG", "Minor grid green channel", QVariant::Double, 0.25, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/MinorColorB", "Minor grid blue channel", QVariant::Double, 0.25, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/MinorColorA", "Minor grid alpha channel", QVariant::Double, 0.4, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/AxisColorR", "Axis red channel", QVariant::Double, 0.9, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/AxisColorG", "Axis green channel", QVariant::Double, 0.3, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/AxisColorB", "Axis blue channel", QVariant::Double, 0.3, 0.0, 1.0});
    schema.registerProperty({"UI/CompositionGrid/AxisColorA", "Axis alpha channel", QVariant::Double, 0.9, 0.0, 1.0});
    schema.applyDefaultsToLayer(ConfigLayer::System);
}
}

class ArtifactAppSettings::Impl {
public:
    LayeredConfigStore& store;
    bool safeMode = false;

    Impl() : store(LayeredConfigStore::instance()) {
        registerBuiltInConfigSchema();
        store.setValidator([](std::string_view key, const QVariant& value) {
            const auto& schema = ConfigSchema::instance();
            const auto* property = schema.find(key);
            if (!property) return true;
            return schema.validate(key, value).valid;
        });
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

double ArtifactAppSettings::timelineMotionBlurShutterAngle() const {
    return std::clamp(impl_->store.value(
        QStringLiteral("UI/Timeline/MotionBlurShutterAngle"), 180.0).toDouble(),
        0.0, 720.0);
}

void ArtifactAppSettings::setTimelineMotionBlurShutterAngle(double degrees) {
    impl_->store.setValue(QStringLiteral("UI/Timeline/MotionBlurShutterAngle"),
                          std::clamp(std::isfinite(degrees) ? degrees : 180.0,
                                     0.0, 720.0));
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::timelineMotionBlurSampleCount() const {
    return std::clamp(static_cast<int>(impl_->store.valueInt64(
        QStringLiteral("UI/Timeline/MotionBlurSampleCount"), 8)), 1, 32);
}

void ArtifactAppSettings::setTimelineMotionBlurSampleCount(int count) {
    impl_->store.setValue(QStringLiteral("UI/Timeline/MotionBlurSampleCount"),
                          std::clamp(count, 1, 32));
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

bool ArtifactAppSettings::accessibilityStickyKeysEnabled() const {
    return impl_->store.valueBool(QStringLiteral("Accessibility/StickyKeysEnabled"), false);
}

void ArtifactAppSettings::setAccessibilityStickyKeysEnabled(bool enable) {
    impl_->store.setValue(QStringLiteral("Accessibility/StickyKeysEnabled"), enable);
    Q_EMIT settingsChanged();
}

QString ArtifactAppSettings::accessibilityStickyKeysMode() const {
    const QString mode = impl_->store.valueString(
        QStringLiteral("Accessibility/StickyKeysMode"), QStringLiteral("latch"));
    return mode == QStringLiteral("lock") || mode == QStringLiteral("both")
        ? mode : QStringLiteral("latch");
}

void ArtifactAppSettings::setAccessibilityStickyKeysMode(const QString& mode) {
    const QString normalized = mode == QStringLiteral("lock") || mode == QStringLiteral("both")
        ? mode : QStringLiteral("latch");
    impl_->store.setValue(QStringLiteral("Accessibility/StickyKeysMode"), normalized);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::accessibilitySingleHandModeEnabled() const {
    return impl_->store.valueBool(QStringLiteral("Accessibility/SingleHandModeEnabled"), false);
}

void ArtifactAppSettings::setAccessibilitySingleHandModeEnabled(bool enable) {
    impl_->store.setValue(QStringLiteral("Accessibility/SingleHandModeEnabled"), enable);
    Q_EMIT settingsChanged();
}

bool ArtifactAppSettings::accessibilityViewportMagnifierEnabled() const {
    return impl_->store.valueBool(QStringLiteral("Accessibility/ViewportMagnifierEnabled"), false);
}

void ArtifactAppSettings::setAccessibilityViewportMagnifierEnabled(bool enable) {
    impl_->store.setValue(QStringLiteral("Accessibility/ViewportMagnifierEnabled"), enable);
    Q_EMIT settingsChanged();
}

int ArtifactAppSettings::accessibilityViewportMagnifierScale() const {
    return std::clamp(static_cast<int>(impl_->store.valueInt64(
        QStringLiteral("Accessibility/ViewportMagnifierScale"), 2)), 2, 8);
}

void ArtifactAppSettings::setAccessibilityViewportMagnifierScale(int scale) {
    impl_->store.setValue(QStringLiteral("Accessibility/ViewportMagnifierScale"),
                          std::clamp(scale, 2, 8));
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
