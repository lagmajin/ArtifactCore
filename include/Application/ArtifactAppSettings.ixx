module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <QString>
#include <QVariant>
#include <wobjectdefs.h>

export module Application.AppSettings;

import Artifact.Grid.System;
import Core.FastSettingsStore;

namespace ArtifactCore {

export class LIBRARY_DLL_API ArtifactAppSettings : public QObject {
    W_OBJECT(ArtifactAppSettings)
public:
    static ArtifactAppSettings* instance();

    // --- General Settings ---
    QString defaultFontFamily() const;
    void setDefaultFontFamily(const QString& family);

    int autoSaveIntervalMinutes() const;
    void setAutoSaveIntervalMinutes(int minutes);

    bool loadLastProjectOnStartup() const;
    void setLoadLastProjectOnStartup(bool enable);

    int menuBarFontScalePercent() const;
    void setMenuBarFontScalePercent(int percent);

    int dockTabFontPointSize() const;
    void setDockTabFontPointSize(int pointSize);

    // --- Composition View Defaults ---
    float compositionCheckerboardSize() const;
    void setCompositionCheckerboardSize(float size);
    Artifact::Grid::GridSettings compositionGridSettings() const;
    void setCompositionGridSettings(const Artifact::Grid::GridSettings& settings);
    int compositionBackgroundMode() const;
    void setCompositionBackgroundMode(int mode);
    bool compositionShowGrid() const;
    void setCompositionShowGrid(bool enable);
    bool compositionShowGuides() const;
    void setCompositionShowGuides(bool enable);
    bool compositionShowSafeMargins() const;
    void setCompositionShowSafeMargins(bool enable);
    bool compositionShowAnchorCenterOverlay() const;
    void setCompositionShowAnchorCenterOverlay(bool enable);
    bool compositionShowCameraFrustumOverlay() const;
    void setCompositionShowCameraFrustumOverlay(bool enable);
    bool compositionShowMotionPathOverlay() const;
    void setCompositionShowMotionPathOverlay(bool enable);

    // --- Import / Preview Defaults ---
    QString importDefaultFrameRateText() const;
    void setImportDefaultFrameRateText(const QString& value);
    QString importColorSpaceText() const;
    void setImportColorSpaceText(const QString& value);
    QString importAudioSampleRateText() const;
    void setImportAudioSampleRateText(const QString& value);
    bool importAutoDetectAlpha() const;
    void setImportAutoDetectAlpha(bool enable);
    bool importInterpretFootage() const;
    void setImportInterpretFootage(bool enable);
    QString importFieldOrderText() const;
    void setImportFieldOrderText(const QString& value);
    int importStillImageDurationSeconds() const;
    void setImportStillImageDurationSeconds(int seconds);
    bool importCreateCompositionOnImport() const;
    void setImportCreateCompositionOnImport(bool enable);

    // --- Project Defaults ---
    int projectDefaultCompositionWidth() const;
    void setProjectDefaultCompositionWidth(int width);
    int projectDefaultCompositionHeight() const;
    void setProjectDefaultCompositionHeight(int height);
    double projectDefaultCompositionFrameRate() const;
    void setProjectDefaultCompositionFrameRate(double fps);
    QString projectDefaultCompositionBackgroundColor() const;
    void setProjectDefaultCompositionBackgroundColor(const QString& value);
    QString projectDefaultWorkspaceModeText() const;
    void setProjectDefaultWorkspaceModeText(const QString& value);

    QString previewQualityText() const;
    void setPreviewQualityText(const QString& value);
    int previewResolutionPercent() const;
    void setPreviewResolutionPercent(int percent);
    bool previewEnableRamCache() const;
    void setPreviewEnableRamCache(bool enable);
    int previewCacheSizeMB() const;
    void setPreviewCacheSizeMB(int value);
    bool previewEnableDiskCache() const;
    void setPreviewEnableDiskCache(bool enable);
    bool previewGenerateThumbnails() const;
    void setPreviewGenerateThumbnails(bool enable);
    QString previewThumbnailQualityText() const;
    void setPreviewThumbnailQualityText(const QString& value);
    bool previewEnableGpuAcceleration() const;
    void setPreviewEnableGpuAcceleration(bool enable);
    QString previewGpuDeviceText() const;
    void setPreviewGpuDeviceText(const QString& value);

    // --- UI Settings ---
    QString themeName() const;
    void setThemeName(const QString& theme);
    QString themePresetPath() const;
    void setThemePresetPath(const QString& path);

    // --- Render Settings ---
    int renderThreadCount() const;
    void setRenderThreadCount(int count);
    bool toolbarShowGrid() const;
    void setToolbarShowGrid(bool enable);
    bool toolbarShowGuide() const;
    void setToolbarShowGuide(bool enable);

    // --- Runtime States (Not persisted) ---
    bool isSafeMode() const;
    void setSafeMode(bool enable);

    // --- Persistence ---
    void sync();

    ~ArtifactAppSettings();

signals:
    void settingsChanged() W_SIGNAL(settingsChanged);

private:
    ArtifactAppSettings();

    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
