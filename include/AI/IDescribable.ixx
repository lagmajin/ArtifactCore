module;
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <functional>

export module Core.AI.Describable;

export namespace ArtifactCore {

/**
 * @brief Supported languages for AI descriptions
 */
enum class DescriptionLanguage {
    English,    // en
    Japanese,   // ja
    Chinese,    // zh
    Korean,     // ko (future)
    Spanish,    // es (future)
    Auto        // Use system default
};

/**
 * @brief Language-aware text container
 */
struct LocalizedText {
    QString en;     // English
    QString ja;     // Japanese
    QString zh;     // Chinese (Simplified)
    
    QString get(DescriptionLanguage lang) const {
        switch (lang) {
            case DescriptionLanguage::Japanese: return ja.isEmpty() ? en : ja;
            case DescriptionLanguage::Chinese: return zh.isEmpty() ? en : zh;
            default: return en;
        }
    }
    
    QString operator()(DescriptionLanguage lang = DescriptionLanguage::English) const {
        return get(lang);
    }
    
    // Builder pattern for convenient construction
    LocalizedText& setEn(const QString& text) { en = text; return *this; }
    LocalizedText& setJa(const QString& text) { ja = text; return *this; }
    LocalizedText& setZh(const QString& text) { zh = text; return *this; }
};

/**
 * @brief Property description for AI context
 */
struct PropertyDescription {
    QString name;
    LocalizedText description;
    QString type;
    QString defaultValue;
    QString minValue;
    QString maxValue;
    QStringList possibleValues;
    LocalizedText notes;
    
    QJsonObject toJson(DescriptionLanguage lang = DescriptionLanguage::English) const {
        QJsonObject json;
        json["name"] = name;
        json["description"] = description.get(lang);
        json["type"] = type;
        if (!defaultValue.isEmpty()) json["defaultValue"] = defaultValue;
        if (!minValue.isEmpty()) json["minValue"] = minValue;
        if (!maxValue.isEmpty()) json["maxValue"] = maxValue;
        if (!possibleValues.isEmpty()) {
            QJsonArray values;
            for (const auto& v : possibleValues) values.append(v);
            json["possibleValues"] = values;
        }
        if (!notes.get(lang).isEmpty()) json["notes"] = notes.get(lang);
        return json;
    }
};

/**
 * @brief Method description for AI context
 */
struct MethodDescription {
    QString name;
    LocalizedText description;
    QString returnType;
    QStringList parameterTypes;
    QStringList parameterNames;
    LocalizedText returnDescription;
    QStringList exampleUsage;
    
    QJsonObject toJson(DescriptionLanguage lang = DescriptionLanguage::English) const {
        QJsonObject json;
        json["name"] = name;
        json["description"] = description.get(lang);
        json["returnType"] = returnType;
        
        QJsonArray params;
        for (int i = 0; i < parameterNames.size() && i < parameterTypes.size(); i++) {
            QJsonObject param;
            param["name"] = parameterNames[i];
            param["type"] = parameterTypes[i];
            params.append(param);
        }
        json["parameters"] = params;
        
        if (!returnDescription.get(lang).isEmpty()) {
            json["returnDescription"] = returnDescription.get(lang);
        }
        
        if (!exampleUsage.isEmpty()) {
            QJsonArray examples;
            for (const auto& ex : exampleUsage) examples.append(ex);
            json["examples"] = examples;
        }
        
        return json;
    }
};

/**
 * @brief Interface for AI-describable objects
 * 
 * Classes implementing this interface can provide contextual descriptions
 * for AI assistants in multiple languages.
 */
class IDescribable {
public:
    virtual ~IDescribable() = default;
    
    // ===== Basic Description =====
    
    /**
     * @brief Get class name
     */
    virtual QString className() const = 0;
    
    /**
     * @brief Get brief description (one sentence)
     */
    virtual LocalizedText briefDescription() const = 0;
    
    /**
     * @brief Get detailed description (paragraph)
     */
    virtual LocalizedText detailedDescription() const {
        return briefDescription();
    }
    
    // ===== Properties =====
    
    /**
     * @brief Get list of property descriptions
     */
    virtual QList<PropertyDescription> propertyDescriptions() const {
        return {};
    }
    
    /**
     * @brief Get property description by name
     */
    virtual PropertyDescription propertyDescription(const QString& name) const {
        for (const auto& prop : propertyDescriptions()) {
            if (prop.name == name) return prop;
        }
        return PropertyDescription();
    }
    
    // ===== Methods =====
    
    /**
     * @brief Get list of method descriptions
     */
    virtual QList<MethodDescription> methodDescriptions() const {
        return {};
    }
    
    /**
     * @brief Get method description by name
     */
    virtual MethodDescription methodDescription(const QString& name) const {
        for (const auto& method : methodDescriptions()) {
            if (method.name == name) return method;
        }
        return MethodDescription();
    }
    
    // ===== Usage =====
    
    /**
     * @brief Get usage examples
     */
    virtual LocalizedText usageExamples() const {
        return {};
    }
    
    /**
     * @brief Get common use cases
     */
    virtual QList<LocalizedText> useCases() const {
        return {};
    }
    
    /**
     * @brief Get related classes
     */
    virtual QStringList relatedClasses() const {
        return {};
    }
    
    // ===== AI Context =====
    
    /**
     * @brief Get AI-friendly description in specified language
     */
    virtual QString describeForAI(DescriptionLanguage lang = DescriptionLanguage::English) const {
        QString result;
        
        // Class name and brief
        result += QString("# %1\n\n").arg(className());
        result += briefDescription().get(lang) + "\n\n";
        
        // Detailed description
        auto detailed = detailedDescription().get(lang);
        if (!detailed.isEmpty()) {
            result += "## Description\n\n" + detailed + "\n\n";
        }
        
        // Properties
        auto props = propertyDescriptions();
        if (!props.isEmpty()) {
            result += "## Properties\n\n";
            for (const auto& prop : props) {
                result += QString("- **%1** (%2): %3\n")
                    .arg(prop.name)
                    .arg(prop.type)
                    .arg(prop.description.get(lang));
            }
            result += "\n";
        }
        
        // Methods
        auto methods = methodDescriptions();
        if (!methods.isEmpty()) {
            result += "## Methods\n\n";
            for (const auto& method : methods) {
                result += QString("- **%1()**: %2\n")
                    .arg(method.name)
                    .arg(method.description.get(lang));
            }
            result += "\n";
        }
        
        // Usage examples
        auto examples = usageExamples().get(lang);
        if (!examples.isEmpty()) {
            result += "## Examples\n\n```\n" + examples + "\n```\n\n";
        }
        
        // Related classes
        auto related = relatedClasses();
        if (!related.isEmpty()) {
            result += "## Related\n\n";
            for (const auto& cls : related) {
                result += QString("- %1\n").arg(cls);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get structured JSON description for AI
     */
    virtual QJsonObject toJsonDescription(DescriptionLanguage lang = DescriptionLanguage::English) const {
        QJsonObject json;
        
        json["className"] = className();
        json["briefDescription"] = briefDescription().get(lang);
        json["detailedDescription"] = detailedDescription().get(lang);
        
        // Properties
        QJsonArray propsArray;
        for (const auto& prop : propertyDescriptions()) {
            propsArray.append(prop.toJson(lang));
        }
        json["properties"] = propsArray;
        
        // Methods
        QJsonArray methodsArray;
        for (const auto& method : methodDescriptions()) {
            methodsArray.append(method.toJson(lang));
        }
        json["methods"] = methodsArray;
        
        // Use cases
        QJsonArray useCasesArray;
        for (const auto& uc : useCases()) {
            useCasesArray.append(uc.get(lang));
        }
        json["useCases"] = useCasesArray;
        
        // Related
        QJsonArray relatedArray;
        for (const auto& cls : relatedClasses()) {
            relatedArray.append(cls);
        }
        json["relatedClasses"] = relatedArray;
        
        return json;
    }
    
    // ===== Language Helpers =====
    
    /**
     * @brief Create a localized text with all languages
     */
    static LocalizedText loc(const QString& en, const QString& ja = {}, const QString& zh = {}) {
        LocalizedText text;
        text.en = en;
        text.ja = ja;
        text.zh = zh;
        return text;
    }
};

/**
 * @brief Helper macro for creating property descriptions
 */
#define DESCRIBE_PROPERTY(name, type, descEn, descJa, descZh) \
    PropertyDescription { \
        QString(#name), \
        IDescribable::loc(descEn, descJa, descZh), \
        QString(#type) \
    }

/**
 * @brief Helper class for building descriptions fluently
 */
class DescriptionBuilder {
public:
    DescriptionBuilder& setClassName(const QString& name) {
        className_ = name;
        return *this;
    }
    
    DescriptionBuilder& setBrief(const QString& en, const QString& ja = {}, const QString& zh = {}) {
        brief_ = IDescribable::loc(en, ja, zh);
        return *this;
    }
    
    DescriptionBuilder& setDetailed(const QString& en, const QString& ja = {}, const QString& zh = {}) {
        detailed_ = IDescribable::loc(en, ja, zh);
        return *this;
    }
    
    DescriptionBuilder& addProperty(const QString& name, const QString& type,
                                   const QString& descEn, const QString& descJa = {}, const QString& descZh = {}) {
        PropertyDescription prop;
        prop.name = name;
        prop.type = type;
        prop.description = IDescribable::loc(descEn, descJa, descZh);
        properties_.append(prop);
        return *this;
    }
    
    DescriptionBuilder& addMethod(const QString& name, const QString& returnType,
                                  const QString& descEn, const QString& descJa = {}, const QString& descZh = {}) {
        MethodDescription method;
        method.name = name;
        method.returnType = returnType;
        method.description = IDescribable::loc(descEn, descJa, descZh);
        methods_.append(method);
        return *this;
    }
    
    DescriptionBuilder& addUseCase(const QString& en, const QString& ja = {}, const QString& zh = {}) {
        useCases_.append(IDescribable::loc(en, ja, zh));
        return *this;
    }
    
    DescriptionBuilder& addRelated(const QString& className) {
        related_.append(className);
        return *this;
    }
    
    QString className_;
    LocalizedText brief_;
    LocalizedText detailed_;
    QList<PropertyDescription> properties_;
    QList<MethodDescription> methods_;
    QList<LocalizedText> useCases_;
    QStringList related_;
};

/**
 * @brief Manager for collecting descriptions from all describable objects
 */
class DescriptionRegistry {
public:
    static DescriptionRegistry& instance() {
        static DescriptionRegistry registry;
        return registry;
    }
    
    void registerDescribable(const QString& name, std::function<const IDescribable*()> getter) {
        descriptors_[name] = getter;
    }
    
    QStringList registeredClasses() const {
        return descriptors_.keys();
    }
    
    QString describeAll(DescriptionLanguage lang = DescriptionLanguage::English) const {
        QString result;
        for (auto it = descriptors_.constBegin(); it != descriptors_.constEnd(); ++it) {
            const IDescribable* obj = it.value()();
            if (obj) {
                result += obj->describeForAI(lang) + "\n\n---\n\n";
            }
        }
        return result;
    }
    
    QJsonObject describeAllAsJson(DescriptionLanguage lang = DescriptionLanguage::English) const {
        QJsonObject json;
        for (auto it = descriptors_.constBegin(); it != descriptors_.constEnd(); ++it) {
            const IDescribable* obj = it.value()();
            if (obj) {
                json[it.key()] = obj->toJsonDescription(lang);
            }
        }
        return json;
    }
    
private:
    QMap<QString, std::function<const IDescribable*()>> descriptors_;
};

/**
 * @brief Helper for automatic registration
 */
template<typename T>
class AutoRegisterDescribable {
public:
    AutoRegisterDescribable(const QString& name) {
        DescriptionRegistry::instance().registerDescribable(name, []() -> const IDescribable* {
            static T instance;
            return &instance;
        });
    }
};

// Convenience macro for registration
#define REGISTER_DESCRIBABLE(Class) \
    static AutoRegisterDescribable<Class> _reg_##Class(#Class)

} // namespace ArtifactCore