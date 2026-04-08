module;
#include <utility>
export module Input.Operator.Manager;
#include <QObject>

import Input.Operator;

export namespace ArtifactCore
{

 class InputOperatorManager {
 private:
  class Impl;
  Impl* impl_;
 public:
  InputOperatorManager();
  ~InputOperatorManager();
  
 };

}
