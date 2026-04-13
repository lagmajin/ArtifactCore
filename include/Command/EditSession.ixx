module;
#include <memory>
#include <utility>
#include <vector>
#include <functional>
#include "../Define/DllExportMacro.hpp"
#include <QString>
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUndoStack>

export module Command.Session;

import Command.Serializable;
import Network.CollaborationWebSocket;

export namespace ArtifactCore
{
    /**
     * @brief 編集セッションを管理するクラス
     * 変更履歴を追跡し、コラボレーション用に同期する準備をします。
     */
    class LIBRARY_DLL_API EditSession : public QObject
    {
        Q_OBJECT
    public:
        EditSession(QObject* parent = nullptr) : QObject(parent) {}

        /**
         * @brief コマンドを実行し、履歴に追加する
         */
        void pushCommand(std::unique_ptr<SerializableCommand> command)
        {
            // ここでシリアライズして送信待ちキューに入れたり、ログに記録したりする
            QJsonObject logEntry;
            logEntry.insert(QStringLiteral("type"), QJsonValue(command->commandType()));
            logEntry.insert(QStringLiteral("data"), QJsonValue(command->serialize()));
            historyLog_.push_back(logEntry);

            undoStack_.push(static_cast<QUndoCommand*>(command.release())); // 所有権の委譲

            // コラボレーション有効時はリモートにブロードキャスト
            if (isCollaborationEnabled_ && collabClient_ && !isRemoteOperation_) {
                broadcastCommand(logEntry);
            }
        }

        /**
         * @brief リモート操作を適用（ローカルUndoStackには積まない）
         */
        void applyRemoteOperation(const QJsonObject& operation)
        {
            isRemoteOperation_ = true;
            historyLog_.push_back(operation);
            // リモート操作はUndoStackに積まない（ローカルのUndo/Redoと分離）
            isRemoteOperation_ = false;

            emit remoteOperationApplied(operation);
        }

        QUndoStack* undoStack() { return &undoStack_; }

        /**
         * @brief 全履歴をJSONとして取得（フル同期用）
         */
        QJsonArray getHistoryAsJson() const
        {
            QJsonArray arr;
            for (const auto& entry : historyLog_) {
                arr.append(entry);
            }
            return arr;
        }

        // コラボレーション機能
        void setCollaborationClient(std::shared_ptr<CollaborationWebSocket> client)
        {
            collabClient_ = std::move(client);
        }

        void setCollaborationEnabled(bool enabled)
        {
            isCollaborationEnabled_ = enabled;
        }

        bool isCollaborationEnabled() const { return isCollaborationEnabled_; }
        bool isRemoteOperation() const { return isRemoteOperation_; }

        void broadcastCommand(const QJsonObject& cmd)
        {
            if (collabClient_) {
                collabClient_->sendOperation(cmd);
            }
        }

    signals:
        void remoteOperationApplied(const QJsonObject& operation);

    private:
        QUndoStack undoStack_;
        std::vector<QJsonObject> historyLog_;
        std::shared_ptr<CollaborationWebSocket> collabClient_;
        bool isCollaborationEnabled_ = false;
        bool isRemoteOperation_ = false;
    };
}
