module;
#include <utility>
#include <string>
#include <unordered_map>
#include <memory>
#include <QString>

export module Core.Localization;

import Utils.String.UniString;

export namespace ArtifactCore {

/**
 * @brief 言語コードの定義
 */
enum class LocaleLanguage {
    English,             // en
    Japanese,            // ja
    ChineseSimplified,   // zh-CN / zh
    ChineseTraditional,  // zh-TW
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
    LocaleLanguage language() const;
    QString languageCode() const;

    // 翻訳の実行
    // キーが見つからない場合はキー自身を返す
    QString translate(const QString& key) const;
    
    // データ登録
    void addTranslation(LocaleLanguage lang, const QString& key, const QString& value);

    // 外部ファイルからロード
    bool loadFromFile(const std::string& path, LocaleLanguage lang);

    // ディレクトリから全言語を一括ロード
    bool loadFromDirectory(const QString& dirPath);
};

} // namespace ArtifactCore

/**
 * @brief 翻訳用ヘルパーマクロ
 */
#define AT_TR(key) ArtifactCore::LocalizationManager::instance().translate(key)
