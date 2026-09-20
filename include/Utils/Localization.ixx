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
 * @brief 複数形のカテゴリ（CLDR Plural Rules の部分集合）
 */
enum class PluralCategory {
    Zero,
    One,
    Two,
    Few,
    Many,
    Other
};

/**
 * @brief 指定ロケール・数量に対する CLDR 複数形カテゴリを返す
 *
 * 現時点で明示対応するのは en / ru / ar。それ以外は One / Other の二値。
 */
PluralCategory pluralCategoryFor(LocaleLanguage lang, double count);

/**
 * @brief 翻訳カタログがホットリロードされたことを通知するイベント
 *
 * `reloadedLocales` は再読み込み後に利用可能なロケールコードのリスト。
 * 購読者は変更後のカタログから再取得する（UIの再翻訳は購読者の責務）。
 */
struct TranslationsReloadedEvent {
    QStringList reloadedLocales;
};

/**
 * @brief アプリケーションの言語が切り替えられたことを通知するイベント
 *
 * `locale` は新しくアクティブになった言語コード（"ja" / "en" 等）
 * App側設定ダイアログの確認時やコマンドライン `--lang` による即時切替時に発火。
 * 購読者は on-demand で各ウィジェットを再翻訳する。
 */
struct LocaleChangedEvent {
    QString locale;
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
    // 指定ロケールコードで読み込まれている翻訳キー数（未ロードなら 0）
    int translationCount(const QString& localeCode) const;

    // 翻訳の実行
    // キーが見つからない場合はキー自身を返す
    QString translate(const QString& key) const;

    // 現在の言語で複数形カテゴリを解決する
    PluralCategory pluralCategory(double count) const;

    // baseKey.<category> を引き、無ければ baseKey.other、最後に呼出側フォールバックを使う。
    // 値の中の %1 は count に置換される。
    QString translatePlural(const QString& baseKey, double count,
                            const QString& fallbackSingular,
                            const QString& fallbackPlural) const;
    
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

    // ホットリロード：ロケールディレクトリの再読み込みと変更通知
    void setLocaleDirectory(const QString& dirPath);
    void reload();
};

} // namespace ArtifactCore

/**
 * @brief 翻訳用ヘルパーマクロ
 */
#define AT_TR(key) ArtifactCore::LocalizationManager::instance().translate(key)
