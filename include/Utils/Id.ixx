module;
#include <QDebug>
#include <QString>
#include <QByteArray> 
#include <boost/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>  
#include <boost/uuid/uuid_generators.hpp>

#include "../Define/DllExportMacro.hpp"
    
export module Utils.Id;

import std;

export namespace ArtifactCore{

class  LIBRARY_DLL_API Id {
 public:
  Id();
  explicit Id(const QString& s);
  explicit Id(const boost::uuids::uuid& u);
  Id(const Id& other);
  Id(Id&& other) noexcept;
  ~Id();

  Id& operator=(const Id& other);

  Id& operator=(Id&& other) noexcept;
  QString toString() const;
  bool isNil() const;
  const boost::uuids::uuid& getUuid() const;

  // 比較演算子
  bool operator==(const Id& other) const;
  bool operator!=(const Id& other) const;
  bool operator<(const Id& other) const;

 private:
  class Impl;// 実装クラスへの生ポインタ
  Impl* impl_;
 };






};