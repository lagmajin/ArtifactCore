module;
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <wobjectimpl.h>

module Application.AppSettings;

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

void ArtifactAppSettings::sync() {
    impl_->store.sync();
}

} // namespace ArtifactCore
