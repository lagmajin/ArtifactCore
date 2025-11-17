module;

#include <QUndoCommand>
export module Command.Lambda;

export namespace ArtifactCore {

 class LambdaCommand :public QUndoCommand {
 private:
  class Impl;
  Impl* impl_;

 public:
  explicit LambdaCommand(std::function<void()> doFunc,
   std::function<void()> undoFunc = nullptr,
   const QString& text = QString(),
   QUndoCommand* parent = nullptr);
  ~LambdaCommand();
  void undo() override;

  void redo() override;
 };

};