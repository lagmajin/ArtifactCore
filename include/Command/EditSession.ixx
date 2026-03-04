module;
#include <QUndoStack>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Command.Session;

import Command.Serializable;

export namespace ArtifactCore
{
    /**
     * @brief 編集セッションを管理するクラス
     * 変更履歴を追跡し、コラボレーション用に同期する準備をします。
     */
    class LIBRARY_DLL_API EditSession : public QObject
    {
    public:
        EditSession(QObject* parent = nullptr) : QObject(parent) {}

        /**
         * @brief コマンドを実行し、履歴に追加する
         */
        void pushCommand(std::unique_ptr<SerializableCommand> command)
        {
            // ここでシリアライズして送信待ちキューに入れたり、ログに記録したりする
            QJsonObject logEntry;
            logEntry["type"] = command->commandType();
            logEntry["data"] = command->serialize();
            historyLog_.push_back(logEntry);

            undoStack_.push(command.release()); // 所有権の委譲
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

    private:
        QUndoStack undoStack_;
        std::vector<QJsonObject> historyLog_;
    };
}
