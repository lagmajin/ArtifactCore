module;
#include <QString>
#include <QUndoStack>
module Command.Lambda;

import std;


namespace ArtifactCore {

 class LambdaCommand::Impl{
 private:

 public:
  std::function<void()> doFunc_;
  std::function<void()> undoFunc_;
 
 };

 LambdaCommand::LambdaCommand(std::function<void()> doFunc, std::function<void()> undoFunc /*= nullptr*/, const QString& text /*= QString()*/, QUndoCommand* parent /*= nullptr*/):QUndoCommand(text, parent), impl_(new Impl{ std::move(doFunc), std::move(undoFunc) }) 
 {
 
 }
 
 LambdaCommand::~LambdaCommand()
 {
  delete impl_;
 }

 void LambdaCommand::undo()
 {
  
 }

 void LambdaCommand::redo()
 {
  
 }

}