module;
#include <QDebug>
#include <QString>
#include <QByteArray> 
#include <wobjectdefs.h>


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


class LIBRARY_DLL_API CompositionID : public Id {
public:
 using Id::Id; // Idのコンストラクタを継承
};

class LIBRARY_DLL_API LayerID : public Id {
public:
 using Id::Id;
};



inline uint qHash(const Id& key, uint seed = 0) noexcept
{
 const auto& uuid = key.getUuid();
 uint64_t data64[2];
 static_assert(sizeof(uuid.data) == 16, "UUID must be 16 bytes");
 std::memcpy(data64, uuid.data, 16);

 uint h1 = static_cast<uint>(data64[0] ^ (data64[0] >> 32));
 uint h2 = static_cast<uint>(data64[1] ^ (data64[1] >> 32));

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
 return qHashMulti(seed, h1, h2);
#else
 return qHash(h1 ^ h2 ^ seed);
#endif
}

};