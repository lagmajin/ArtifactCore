module;
#include <QString>
#include <QtNetwork/QTcpServer>

export module NetworkRPCServer;

export namespace ArtifactCore
{


 class NetworkPCServer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  NetworkPCServer();
  ~NetworkPCServer();
  bool start(unsigned short port);
  void stop();

  QString call(const QString& function, const QStringList& args);

 };









};