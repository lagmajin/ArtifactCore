module;
#include <utility>
#include <memory>
#include <QJsonObject>
#include <QString>
#include <QJsonValue>

export module Command.Common;

import Command.Serializable;
import Utils.Id;

export namespace ArtifactCore
{
    /**
     * @brief プロパティ変更を同期するための汎用コマンド
     */
    class PropertyChangeCommand : public SerializableCommand
    {
    public:
        PropertyChangeCommand() : SerializableCommand() {}
        PropertyChangeCommand(const Id& targetId, const QString& propertyName, const QJsonValue& newValue, const QJsonValue& oldValue)
            : targetId_(targetId), propertyName_(propertyName), newValue_(newValue), oldValue_(oldValue)
        {
            setText(QString("Change %1 of %2").arg(propertyName).arg(targetId.toString()));
        }

        QString commandType() const override { return "PropertyChange"; }

        QJsonObject serialize() const override
        {
            QJsonObject data;
            data["targetId"] = targetId_.toString();
            data["propertyName"] = propertyName_;
            data["newValue"] = newValue_.toVariant().toJsonValue(); // 簡易化のため
            data["oldValue"] = oldValue_.toVariant().toJsonValue();
            return data;
        }

        bool deserialize(const QJsonObject& data) override
        {
            targetId_ = Id(data["targetId"].toString());
            propertyName_ = data["propertyName"].toString();
            newValue_ = data["newValue"];
            oldValue_ = data["oldValue"];
            return true;
        }

        void undo() override
        {
            // ここで実際にオブジェクトを見つけ出し、値を戻す処理を将来的に実装
        }

        void redo() override
        {
            // ここで実際に値を設定する処理を将来的に実装
        }

    private:
        Id targetId_;
        QString propertyName_;
        QJsonValue newValue_;
        QJsonValue oldValue_;
    };
}
