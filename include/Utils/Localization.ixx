module;
#include <utility>
#include <unordered_map>
#include <memory>
#include <QString>
#include <QStringList>

export module Core.Localization;

import Utils.String.UniString;
import Core.ArtifactString;

export namespace ArtifactCore {

/**
 * @brief 言語コードの定義
 */
enum class LocaleLanguage {
    English,             // en
    Japanese,            // ja
    ChineseSimplified,   // zh-CN / zh
    ChineseTraditional,  // zh-TW
    Korean,              // ko
    French,              // fr
    German,              // de
    Spanish,             // es
    Portuguese,          // pt
    Russian,              // ru
    Arabic,              // ar
    Auto                 // システム設定に従う
};

/**
 * @brief アプリケーション全体の翻訳を管理するクラス
 */
class LocalizationManager {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    LocalizationManager();

public:
    ~LocalizationManager();

    // シングルトンインスタンス
    static LocalizationManager& instance();

    // 言語の設定・取得
    void setLanguage(LocaleLanguage lang);
    void setLanguageCode(const QString& code);
    LocaleLanguage language() const;
    QString languageCode() const;
    QStringList availableLocales() const;

    // 翻訳の実行
    // キーが見つからない場合はキー自身を返す
    QString translate(const QString& key) const;
    
    // データ登録
    void addTranslation(LocaleLanguage lang, const QString& key, const QString& value);

    // 外部ファイルからロード
    bool loadFromFile(const String& path, LocaleLanguage lang);
    bool loadFromFile(const QString& path, LocaleLanguage lang);

    // ディレクトリから全言語を一括ロード
    bool loadFromDirectory(const QString& dirPath);

    // 現在の言語で未定義の英語キーを列挙
    QStringList missingKeys() const;
    // 現在の言語で英語フォールバックと同じ値のキーを列挙
    QStringList untranslatedKeys() const;
    QStringList loadedKeys() const;
    void clearTranslations();
};

} // namespace ArtifactCore

/**
 * @brief 翻訳用ヘルパーマクロ
 */
#define AT_TR(key) ArtifactCore::LocalizationManager::instance().translate(key)
