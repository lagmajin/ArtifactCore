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

namespace ArtifactCore{

 LIBRARY_DLL_API class Id {
 public:
  // デフォルトコンストラクタ：ランダムなUUIDを生成
  Id();

  // QStringからのコンストラクタ
  // 無効な文字列が与えられた場合、nil UUIDを設定します
  explicit Id(const QString& s);
  // boost::uuids::uuid からのコンストラクタ
  explicit Id(const boost::uuids::uuid& u); // 実装をcppファイルに移動
  // コピーコンストラクタ
  Id(const Id& other);
  // ムーブコンストラクタ
  Id(Id&& other) noexcept;
  // デストラクタ
  ~Id();
  // コピー代入演算子
  Id& operator=(const Id& other);

  // ムーブ代入演算子
  Id& operator=(Id&& other) noexcept;

  // UUIDのQString表現を取得
  QString toString() const;

  // UUIDが空（nil）であるかチェック
  bool isNil() const;

  // 内部のboost::uuids::uuidオブジェクトを取得（必要に応じて）
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