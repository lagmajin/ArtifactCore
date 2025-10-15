module;

//#include <QNetwork/QTcpServer>
//#include <QTcpSocket>
#include <QDebug>
//#include <thrift/concurrency/Exception.h>
module NetworkRPCServer;




namespace ArtifactCore
{

 class NetworkPCServer::Impl
 {
 public:

  Impl();
  ~Impl();
 };

 NetworkPCServer::Impl::Impl()
 {

 }

 NetworkPCServer::Impl::~Impl()
 {

 }

 NetworkPCServer::NetworkPCServer() :impl_(new Impl())
 {

 }


 NetworkPCServer::~NetworkPCServer()
 {
  delete impl_;
 }

 void NetworkPCServer::stop()
 {
 }



};