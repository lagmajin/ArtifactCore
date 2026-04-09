module;
#include <utility>
#include <memory>
#include <QString>
#include <QMap>

export module Core.AI.APIKeyManager;

export namespace ArtifactCore {

class APIKeyManager {
public:
    static APIKeyManager& instance();

    void setKey(CloudProvider provider, const QString& key);
    QString getKey(CloudProvider provider) const;
    QString maskedKey(CloudProvider provider) const;
    bool hasKey(CloudProvider provider) const;
    void removeKey(CloudProvider provider);

    void setProxy(CloudProvider provider, const QString& proxyUrl);
    QString getProxy(CloudProvider provider) const;

    void saveToSettings();
    void loadFromSettings();

    void clearAll();

private:
    APIKeyManager() = default;
    ~APIKeyManager() = default;
    APIKeyManager(const APIKeyManager&) = delete;
    APIKeyManager& operator=(const APIKeyManager&) = delete;

    QMap<CloudProvider, QString> apiKeys_;
    QMap<CloudProvider, QString> proxies_;
};

QString cloudProviderToString(CloudProvider provider);
CloudProvider stringToCloudProvider(const QString& str);

} // namespace ArtifactCore