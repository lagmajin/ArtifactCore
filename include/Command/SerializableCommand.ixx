module;
#include <utility>
#include <functional>
#include <map>
#include <memory>
#include "../Define/DllExportMacro.hpp"
#include <QUndoCommand>
#include <QJsonObject>
#include <QString>

export module Command.Serializable;

export namespace ArtifactCore
{
    /**
     * @brief シリアライズ可能なコマンドの基底クラス
     * 将来のコラボレーション機能（ネットワーク経由の同期）の基盤となります。
     */
    class LIBRARY_DLL_API SerializableCommand : public QUndoCommand
    {
    public:
        using QUndoCommand::QUndoCommand;
        virtual ~SerializableCommand() = default;

        /**
         * @brief コマンドの種類を一意に識別する文字列を返します
         */
        virtual QString commandType() const = 0;

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
            creators_[type] = creator;
        }

        std::unique_ptr<SerializableCommand> create(const QString& type)
        {
            if (creators_.count(type)) {
                return creators_[type]();
            }
            return nullptr;
        }

        std::unique_ptr<SerializableCommand> fromJson(const QJsonObject& json)
        {
            QString type = json["type"].toString();
            auto cmd = create(type);
            if (cmd) {
                cmd->deserialize(json["data"].toObject());
            }
            return cmd;
        }

    private:
        std::map<QString, Creator> creators_;
    };
}
