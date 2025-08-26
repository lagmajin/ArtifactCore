module;
#include <QDebug>
#include <QString>
#include <QByteArray> 
#include <boost/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>  
#include <boost/uuid/uuid_generators.hpp>

module Utils.Id;


namespace ArtifactCore {

 class Id::Impl {
 public:
  Impl(); // デフォルトコンストラクタ (ランダムUUID生成)
  explicit Impl(const QString& s); // QStringからのコンストラクタ (パース)
  Impl(const boost::uuids::uuid& uuid_val); // UUID値からのコンストラクタ

  boost::uuids::uuid value_;
 };

 // Id::Impl のデフォルトコンストラクタ - ランダムUUIDを生成
 Id::Impl::Impl() : value_(boost::uuids::random_generator()()) {}

 // Id::Impl のQStringからのコンストラクタ - UUID文字列をパース
 Id::Impl::Impl(const QString& s) {
  std::string s_std = s.toStdString();
  try {
   value_ = boost::uuids::string_generator()(s_std);
  }
  catch (const std::runtime_error& e) {
   qWarning() << "Error parsing UUID QString:" << s << "-" << e.what();
   value_ = boost::uuids::nil_uuid(); // エラー時はnil UUIDを設定
  }
 }

 // Id::Impl のUUID値からのコンストラクタ
 Id::Impl::Impl(const boost::uuids::uuid& uuid_val) : value_(uuid_val) {}


 // --- Id クラスのメソッドの実装 ---

 // デフォルトコンストラクタ：ランダムなUUIDを生成
 Id::Id() : impl_(new Impl()) {}

 // QStringからのコンストラクタ
 Id::Id(const QString& s) : impl_(new Impl(s)) {}

 // boost::uuids::uuid からのコンストラクタ
 Id::Id(const boost::uuids::uuid& u) : impl_(new Impl(u)) {}

 // コピーコンストラクタ
 Id::Id(const Id& other) : impl_(new Impl(other.impl_->value_)) {}

 // ムーブコンストラクタ
 Id::Id(Id&& other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
 }

 // デストラクタ
 Id::~Id() {
  delete impl_;
 }

 // コピー代入演算子
 Id& Id::operator=(const Id& other) {
  if (this != &other) {
   Impl* tmp = new Impl(other.impl_->value_);
   delete impl_;
   impl_ = tmp;
  }
  return *this;
 }
 // ムーブ代入演算子
 Id& Id::operator=(Id&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 // UUIDのQString表現を取得
 QString Id::toString() const {
  return QString::fromStdString(boost::uuids::to_string(impl_->value_));
 }

 // UUIDが空（nil）であるかチェック
 bool Id::isNil() const {
  return impl_->value_.is_nil();
 }

 // 内部のboost::uuids::uuidオブジェクトを取得
 const boost::uuids::uuid& Id::getUuid() const {
  return impl_->value_;
 }

 // 比較演算子
 bool Id::operator==(const Id& other) const {
  return impl_->value_ == other.impl_->value_;
 }

 bool Id::operator!=(const Id& other) const {
  return impl_->value_ != other.impl_->value_;
 }

 bool Id::operator<(const Id& other) const {
  return impl_->value_ < other.impl_->value_;
 }



};