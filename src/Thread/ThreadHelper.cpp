module;

#include <QString>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) || defined(__MACH__)
#include <pthread.h>
#elif defined(__linux__)
#include <pthread.h>
#endif
#include "../../include/Define/DllExportMacro.hpp"


module Thread.Helper;

namespace ArtifactCore
{

 LIBRARY_DLL_API void setCurrentThreadName(const QString& name)
 {
#if defined(_WIN32)
  // Windows: SetThreadDescription は wchar_t* 必須
  std::wstring wname = name.toStdWString();
  SetThreadDescription(GetCurrentThread(), wname.c_str());
#elif defined(__APPLE__) || defined(__MACH__)
  // macOS: 引数は const char*
  pthread_setname_np(name.toUtf8().constData());
#elif defined(__linux__)
  // Linux: 16文字制限あり
  QByteArray ba = name.toUtf8();
  QByteArray truncated = ba.left(15); // 末尾に \0 が入る
  pthread_setname_np(pthread_self(), truncated.constData());
#endif
 }

};