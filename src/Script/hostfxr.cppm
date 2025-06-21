module;
#include <coreclr_delegates.h>
#include <qlogging.h>
#include <QString>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <qversionnumber.h>

module hostfxr;

#pragma comment(lib,"libnethost.lib")

namespace ArtifactCore
{
 typedef int(__cdecl* component_entry_point_fn)(
  const wchar_t* assemblyPath,
  const wchar_t* typeName,
  const wchar_t* methodName,
  const wchar_t* arg,
  void* resultBuffer,
  int bufferLen);


 DotnetRuntimeHost::DotnetRuntimeHost()
 {



 }

 DotnetRuntimeHost::~DotnetRuntimeHost() {
  if (closeFn && hostContext) {
   closeFn(hostContext);
  }
  if (hostfxrLib.isLoaded()) {
   hostfxrLib.unload();
  }
 }
 bool DotnetRuntimeHost::initialize(const QString& dotnetRoot) {
  QDir fxrBaseDir(dotnetRoot + "/host/fxr/");
  if (!fxrBaseDir.exists()) {
   qWarning() << "fxr directory not found at" << fxrBaseDir.absolutePath();
   return false;
  }

  // �o�[�W�������̃f�B���N�g���ꗗ���擾
  QFileInfoList subDirs = fxrBaseDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

  // �o�[�W�������Ń\�[�g
  QVersionNumber latestVersion;
  QString latestVersionDir;

  for (const QFileInfo& info : subDirs) {
   QVersionNumber ver = QVersionNumber::fromString(info.fileName());
   if (ver.isNull()) continue;

   if (QVersionNumber::compare(ver, latestVersion) > 0) {
	latestVersion = ver;
	latestVersionDir = info.absoluteFilePath();
   }
  }

  if (latestVersionDir.isEmpty()) {
   qWarning() << "No valid hostfxr version directories found in" << fxrBaseDir.absolutePath();
   return false;
  }

  QString hostfxrPath = latestVersionDir + "/hostfxr.dll";
  hostfxrLib.setFileName(hostfxrPath);

  if (!hostfxrLib.load()) {
   qWarning() << "Failed to load hostfxr.dll from" << hostfxrPath;
   return false;
  }

  // �֐��|�C���^�擾
  initRuntime = (hostfxr_initialize_for_runtime_config_fn)hostfxrLib.resolve("hostfxr_initialize_for_runtime_config");
  getDelegateFn = (hostfxr_get_runtime_delegate_fn)hostfxrLib.resolve("hostfxr_get_runtime_delegate");
  closeFn = (hostfxr_close_fn)hostfxrLib.resolve("hostfxr_close");

  if (!initRuntime) qWarning() << "resolve failed: hostfxr_initialize_for_runtime_config";
  if (!getDelegateFn) qWarning() << "resolve failed: hostfxr_get_runtime_delegate";
  if (!closeFn) qWarning() << "resolve failed: hostfxr_close";

  if (!initRuntime || !getDelegateFn || !closeFn) {
   qWarning() << "Failed to get one or more hostfxr functions";
   return false;
  }

  return true;
 }

 bool DotnetRuntimeHost::loadAssembly(const QString& assemblyPath) {
  // runtimeconfig.json�̃p�X��assemblyPath���琄�� or �ʓr�n���̂�����


 	QFileInfo dllInfo(assemblyPath);

  if (!dllInfo.exists())
  {
   qWarning() << "dll missing";
  }

  QString baseName = dllInfo.completeBaseName(); // �g���q�Ȃ��̃t�@�C����
  QString dir = dllInfo.absolutePath();

  runtimeConfigPath = QDir(dir).filePath(baseName + ".runtimeconfig.json");

  if (!QFile::exists(runtimeConfigPath)) {
   qWarning() << "runtimeconfig.json not found at" << runtimeConfigPath;
   return false;
  }

  // wchar_t*�ɕϊ�
  std::wstring configPathW = QDir::toNativeSeparators(runtimeConfigPath).toStdWString();

  int rc = initRuntime(configPathW.c_str(), nullptr,&hostContext);
  if (rc != 0 || hostContext == nullptr) {
   qWarning() << "hostfxr_initialize_for_runtime_config failed:" << rc;
   return false;
  }


  //rc=getDelegateFn(hostContext, hdt_load_assembly, (void**)&hostfxr_load_assembly);

  rc = getDelegateFn(
   hostContext,
   hdt_load_assembly,
   (void**)&hostfxr_load_assembly);


  rc = getDelegateFn(
   hostContext,          // from hostfxr_initialize_for_runtime_config
   hdt_get_function_pointer, // usually = 3
   (void**)&hostfxr_get_function_pointer);

  if (rc != 0 || hostfxr_get_function_pointer == nullptr) {
   qWarning() << "hostfxr_get_runtime_delegate failed:" << rc;
   return false;
  }

  qDebug() << "Assembly path:" << assemblyPath;
  qDebug() << "Runtime config path:" << runtimeConfigPath;
  qDebug() << "initRuntime result:" << rc;
  qDebug() << "hostContext is null?" << (hostContext == nullptr);
  // delegate��component_entry_point_fn�̂͂�
  // �L���X�g���ĕۑ��i�^���S�͓K�X�m�F�j

  hostfxr_load_assembly(assemblyPath.toStdWString().c_str(), nullptr, nullptr);


  return true;
 }

 void* DotnetRuntimeHost::getFunctionPointer(const QString& type_name, const QString& funcName)
 {
  void* fn = nullptr;
  const wchar_t* delegateType = L"UNMANAGEDCALLERSONLY";
  int rc = hostfxr_get_function_pointer(
   type_name.toStdWString().c_str(),
   funcName.toStdWString().c_str(),
   UNMANAGEDCALLERSONLY_METHOD,
   nullptr,
   nullptr,
   &fn
  );


 	if (rc != 0) {
   QString error = "�֐��|�C���^�̎擾�Ɏ��s���܂����B";
   return nullptr;
  }

  return fn;
 }











};