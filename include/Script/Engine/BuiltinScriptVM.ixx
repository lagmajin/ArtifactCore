module;
export module Script.Engine.BuiltinVM;

import std;

export namespace ArtifactCore {
 
 class BuiltinScriptVM
 {
 private:
  class Impl;
  Impl* impl_;
	
 public:
  BuiltinScriptVM();
  ~BuiltinScriptVM();
 };

 typedef std::shared_ptr<BuiltinScriptVM> BuiltinScriptVMPtr;
 typedef std::weak_ptr<BuiltinScriptVM>	  BuiltinScriptVMWeakPtr;

};