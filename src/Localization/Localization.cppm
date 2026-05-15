module;
#include <utility>
#include <string>
#include <unordered_map>
#include <memory>
#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>

module Core.Localization;

namespace ArtifactCore {

class LocalizationManager::Impl {
public:
    LocaleLanguage currentLang_ = LocaleLanguage::Auto;
    
    // 言語ごとの翻訳マップ (Language -> (Key -> Value))
    std::unordered_map<LocaleLanguage, std::unordered_map<QString, QString>> translations_;
    
    // フォールバック（英語）
    std::unordered_map<QString, QString> fallback_;

    void flattenJson(const QJsonObject& obj, const QString& prefix, std::unordered_map<QString, QString>& out) {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QString key = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
            if (it->isObject()) {
                flattenJson(it->toObject(), key, out);
            } else if (it->isString()) {
                out[QString(it.key())] = it->toString();
            }
        }
    }
};

LocalizationManager::LocalizationManager() 
    : impl_(std::make_unique<Impl>()) {}

LocalizationManager::~LocalizationManager() = default;

LocalizationManager& LocalizationManager::instance() {
    static LocalizationManager instance;
    return instance;
}

void LocalizationManager::setLanguage(LocaleLanguage lang) {
    impl_->currentLang_ = lang;
    qDebug() << "[Localization] Language set to:" << static_cast<int>(lang);
}

LocaleLanguage LocalizationManager::language() const {
    return impl_->currentLang_;
}

QString LocalizationManager::languageCode() const {
    switch (impl_->currentLang_) {
        case LocaleLanguage::Japanese: return "ja";
        case LocaleLanguage::ChineseSimplified: return "zh";
        case LocaleLanguage::ChineseTraditional: return "zh-TW";
        case LocaleLanguage::English: return "en";
        default: return "en";
    }
}

QString LocalizationManager::translate(const QString& key) const {
    // 現在の言語を検索
    auto langIt = impl_->translations_.find(impl_->currentLang_);
    if (langIt != impl_->translations_.end()) {
        auto valIt = langIt->second.find(key);
        if (valIt != langIt->second.end()) {
            return valIt->second;
        }
    }

    // フォールバック（英語）を検索
    auto fbIt = impl_->fallback_.find(key);
    if (fbIt != impl_->fallback_.end()) {
        return fbIt->second;
    }

    // 見つからなければキーを返す
    return key;
}

void LocalizationManager::addTranslation(LocaleLanguage lang, const QString& key, const QString& value) {
    impl_->translations_[lang][key] = value;
}

bool LocalizationManager::loadFromFile(const std::string& path, LocaleLanguage lang) {
    QString qPath = QString::fromStdString(path);
    QFile file(qPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[Localization] Failed to open:" << qPath;
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[Localization] JSON Parse Error:" << err.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "[Localization] Root is not an object";
        return false;
    }

    // 既存のデータをクリアして読み込み
    impl_->translations_[lang].clear();
    impl_->flattenJson(doc.object(), "", impl_->translations_[lang]);
    
    qDebug() << "[Localization] Loaded" << impl_->translations_[lang].size() << "strings from" << qPath;
    return true;
}

// ディレクトリからの一括ロード
bool LocalizationManager::loadFromDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) return false;

    // 英語をデフォルトフォールバックとしてロード
    QString enPath = dir.filePath("en.json");
    if (QFile::exists(enPath)) {
        loadFromFile(enPath.toStdString(), LocaleLanguage::English);
        impl_->fallback_ = impl_->translations_[LocaleLanguage::English];
    }

    // 利用可能な全言語をロード
    QStringList filters = {"*.json"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const auto& fi : files) {
        QString baseName = fi.baseName();
        LocaleLanguage lang = LocaleLanguage::Auto;
        
        if (baseName == "ja") lang = LocaleLanguage::Japanese;
        else if (baseName == "zh" || baseName == "zh-CN") lang = LocaleLanguage::ChineseSimplified;
        else if (baseName == "zh-TW") lang = LocaleLanguage::ChineseTraditional;
        else if (baseName == "en") lang = LocaleLanguage::English;
        else continue; // サポート外の言語

        if (lang != LocaleLanguage::English) {
            loadFromFile(fi.absoluteFilePath().toStdString(), lang);
        }
    }
    return true;
}

} // namespace ArtifactCore
