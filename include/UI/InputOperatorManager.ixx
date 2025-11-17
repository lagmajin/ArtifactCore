module;
#include <QObject>
export module Input.Operator.Manager;

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