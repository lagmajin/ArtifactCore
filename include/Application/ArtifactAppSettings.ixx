module;
#include <QString>
#include <QVariant>
#include <wobjectdefs.h>
#include "../Define/DllExportMacro.hpp"

export module Application.AppSettings;

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

    // --- UI Settings ---
    QString themeName() const;
    void setThemeName(const QString& theme);

    // --- Render Settings ---
    int renderThreadCount() const;
    void setRenderThreadCount(int count);

    // --- Persistence ---
    void sync();

signals:
    void settingsChanged() W_SIGNAL(settingsChanged);

private:
    ArtifactAppSettings();
    ~ArtifactAppSettings();

    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
