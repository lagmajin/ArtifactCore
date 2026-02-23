module;

#include <QString>
#include <QStringList>
#include <QVariant>

module Core.AI.Describable;

import std;

namespace ArtifactCore {

/// 多言語対応テキスト構造体
struct LocalizedText {
    QString en;  ///< English
    QString ja;  ///< 日本語
    QString zh;  ///< 中文
    
    LocalizedText() = default;
    LocalizedText(const QString& english, const QString& japanese, const QString& chinese)
        : en(english), ja(japanese), zh(chinese) {}
    
    /// 指定言語でテキストを取得
    QString get(const QString& lang = "en") const {
        if (lang == "ja") return ja;
        if (lang == "zh") return zh;
        return en;
    }
};

/// 多言語テキストを簡潔に作成するヘルパー関数
inline LocalizedText loc(const QString& en, const QString& ja, const QString& zh) {
    return LocalizedText(en, ja, zh);
}

/// プロパティ説明構造体
struct PropertyDescription {
    QString name;               ///< プロパティ名
    LocalizedText description;  ///< 説明（多言語）
    QString type;               ///< 型名（int, float, QString, QColor等）
    QString defaultValue;       ///< デフォルト値（文字列表現）
    QString minValue;           ///< 最小値（数値型の場合）
    QString maxValue;           ///< 最大値（数値型の場合）
    
    PropertyDescription() = default;
    PropertyDescription(const QString& n, const LocalizedText& desc, const QString& t,
                        const QString& defVal = {}, const QString& min = {}, const QString& max = {})
        : name(n), description(desc), type(t), defaultValue(defVal), minValue(min), maxValue(max) {}
};

/// メソッド説明構造体
struct MethodDescription {
    QString name;                   ///< メソッド名
    LocalizedText description;      ///< 説明（多言語）
    QString returnType;             ///< 戻り値の型
    QList<QString> parameterTypes;  ///< パラメータの型リスト
    QList<QString> parameterNames;  ///< パラメータ名リスト
    
    MethodDescription() = default;
    MethodDescription(const QString& n, const LocalizedText& desc, const QString& ret,
                      const QList<QString>& paramTypes = {}, const QList<QString>& paramNames = {})
        : name(n), description(desc), returnType(ret), 
          parameterTypes(paramTypes), parameterNames(paramNames) {}
};

/// クラス説明構造体
struct ClassDescription {
    QString className;              ///< クラス名
    LocalizedText briefDescription; ///< 簡潔な説明
    LocalizedText detailedDescription; ///< 詳細説明
    QList<PropertyDescription> properties; ///< プロパティ一覧
    QList<MethodDescription> methods;      ///< メソッド一覧
    QStringList relatedClasses;     ///< 関連クラス一覧
    QStringList tags;               ///< 検索用タグ
    
    ClassDescription() = default;
};

/// 説明可能インターフェース（静的メソッド版）
/// 各クラスがこのトレイトを特殊化して説明を提供
template<typename T>
struct Describable {
    /// クラス説明を取得
    static ClassDescription describe() {
        return ClassDescription{};
    }
};

/// 説明を提供するクラス用の基底クラス
/// 継承して静的メソッド describe() をオーバーライド
class IDescribable {
public:
    virtual ~IDescribable() = default;
    
    /// クラス説明を取得（インスタンスから呼び出し可能）
    virtual ClassDescription describe() const = 0;
    
    /// クラス名を取得
    virtual QString className() const = 0;
};

/// 説明ヘルパーマクロ
/// クラス宣言内で使用して、説明メソッドを自動生成
#define DECLARE_DESCRIPTION \
public: \
    static Core::AI::ClassDescription staticDescribe(); \
    Core::AI::ClassDescription describe() const override { return staticDescribe(); } \
    QString className() const override;

/// 説明実装マクロ
#define IMPLEMENT_DESCRIPTION(ClassName) \
    QString ClassName::className() const { return #ClassName; } \
    Core::AI::ClassDescription ClassName::staticDescribe()

/// 簡易説明登録用（検索API用）
class DescriptionRegistry {
public:
    using DescriptionFactory = std::function<ClassDescription()>;
    
    static DescriptionRegistry& instance() {
        static DescriptionRegistry reg;
        return reg;
    }
    
    void registerDescription(const QString& className, DescriptionFactory factory) {
        m_factories[className] = std::move(factory);
    }
    
    ClassDescription getDescription(const QString& className) const {
        auto it = m_factories.find(className);
        if (it != m_factories.end()) {
            return it.value()();
        }
        return ClassDescription{};
    }
    
    QStringList getAllClassNames() const {
        return m_factories.keys();
    }
    
    QList<ClassDescription> searchByTag(const QString& tag) const {
        QList<ClassDescription> results;
        for (const auto& factory : m_factories) {
            auto desc = factory();
            if (desc.tags.contains(tag)) {
                results.append(desc);
            }
        }
        return results;
    }
    
private:
    QMap<QString, DescriptionFactory> m_factories;
};

/// 自動登録ヘルパー
template<typename T>
class DescriptionAutoRegister {
public:
    DescriptionAutoRegister(const QString& className) {
        DescriptionRegistry::instance().registerDescription(
            className, 
            []() { return Describable<T>::describe(); }
        );
    }
};

/// 自動登録マクロ
#define REGISTER_DESCRIPTION(ClassName) \
    namespace { \
        Core::AI::DescriptionAutoRegister<ClassName> _reg_##ClassName(#ClassName); \
    }

} // namespace ArtifactCore