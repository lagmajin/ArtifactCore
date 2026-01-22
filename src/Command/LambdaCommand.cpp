module;
#include <QString>
#include <QUndoStack>
module Command.Lambda;

import std;


namespace ArtifactCore {

 class LambdaCommand::Impl {
 private:

 public:
  Impl();
  explicit Impl(std::function<void()> doFunc, std::function<void()> undoFunc)
   : doFunc_(std::move(doFunc)), undoFunc_(std::move(undoFunc))
  {
  }
  ~Impl() = default;

  std::function<void()> doFunc_;
  std::function<void()> undoFunc_;
  bool firstTime = true;
 };


 LambdaCommand::LambdaCommand(std::function<void()> doFunc,
  std::function<void()> undoFunc,
  const QString& text,
  QUndoCommand* parent)
  : QUndoCommand(text, parent), impl_(new Impl(std::move(doFunc), std::move(undoFunc)))
 {
 }

 LambdaCommand::~LambdaCommand()
 {
  delete impl_;
 }

 void LambdaCommand::undo()
 {
  if (impl_->undoFunc_) {
   impl_->undoFunc_();
  }
 }

 void LambdaCommand::redo()
 {
  if (impl_->firstTime) {
   impl_->firstTime = false;
   return;
  }
  if (impl_->doFunc_) {
   impl_->doFunc_();
  }
 }

};