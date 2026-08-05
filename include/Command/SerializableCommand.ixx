module;
#include <utility>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include "../Define/DllExportMacro.hpp"
#include <QUndoCommand>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

export module Command.Serializable;

import Serialization.ISerializable;

export namespace ArtifactCore
{
    /**
     * @brief シリアライズ可能なコマンドの基底クラス
     * 将来のコラボレーション機能（ネットワーク経由の同期）の基盤となります。
     */
    class LIBRARY_DLL_API SerializableCommand : public QUndoCommand,
                                                public Serialization::ISerializable
    {
    public:
        using QUndoCommand::QUndoCommand;
        virtual ~SerializableCommand() = default;

        /**
         * @brief コマンドの種類を一意に識別する文字列を返します
         */
        virtual QString commandType() const = 0;

        QString typeName() const override { return commandType(); }
        int schemaVersion() const override { return 1; }

        QJsonObject toJson() const
        {
            return QJsonObject{{QStringLiteral("type"), commandType()},
                               {QStringLiteral("schemaVersion"), schemaVersion()},
                               {QStringLiteral("data"), serialize()}};
        }

        /**
         * @brief コマンドの内容をJSONにシリアライズします
         */
        virtual QJsonObject serialize() const = 0;

        /**
         * @brief JSONからコマンドの状態を復元します
         */
        virtual bool deserialize(const QJsonObject& data) = 0;
    };

    /**
     * @brief JSONからコマンドを復元するためのファクトリ
     */
    class LIBRARY_DLL_API CommandFactory
    {
    public:
        using Creator = std::function<std::unique_ptr<SerializableCommand>()>;

        static CommandFactory& instance()
        {
            static CommandFactory factory;
            return factory;
        }

        void registerCommand(const QString& type, Creator creator)
        {
            if (type.trimmed().isEmpty() || !creator) return;
            std::lock_guard<std::mutex> lock(mutex_);
            creators_[type] = std::move(creator);
        }

        std::unique_ptr<SerializableCommand> create(const QString& type)
        {
            Creator creator;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto it = creators_.find(type);
                if (it == creators_.end()) return nullptr;
                creator = it->second;
            }
            return creator ? creator() : nullptr;
        }

        std::unique_ptr<SerializableCommand> fromJson(const QJsonObject& json)
        {
            const QString type = json.value(QStringLiteral("type")).toString();
            const QJsonValue schemaValue = json.value(QStringLiteral("schemaVersion"));
            const QJsonValue dataValue = json.value(QStringLiteral("data"));
            if (type.trimmed().isEmpty() ||
                (!schemaValue.isUndefined() && schemaValue.toInt(-1) != 1) ||
                !dataValue.isObject()) return nullptr;
            auto cmd = create(type);
            if (!cmd || cmd->commandType() != type ||
                !cmd->deserialize(dataValue.toObject())) return nullptr;
            return cmd;
        }

    private:
        std::mutex mutex_;
        std::map<QString, Creator> creators_;
    };
}
