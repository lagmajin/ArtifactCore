module;
#include <QHash>
#include <QMultiHash>
#include <QVector>

export module Container.MultiIndex;

import std;

export namespace ArtifactCore {


 inline uint qHash(const std::type_index& key, uint seed = 0) noexcept {
  return ::qHash(static_cast<quintptr>(key.hash_code()), seed);
 }

 template <typename Ptr, typename Id, typename TypeKey = std::type_index>
 class MultiIndexContainer {
  // Ptr は必ず std::shared_ptr<T> であることをコンパイル時にチェック
  static_assert(
   std::is_same<Ptr, std::shared_ptr<typename Ptr::element_type>>::value,
   "Ptr must be a std::shared_ptr");

 private:
  QVector<Ptr> list_;               // 線形アクセス
  QHash<Id, Ptr> byId_;             // IDアクセス
  QMultiHash<TypeKey, Ptr> byType_; // 型アクセス
  mutable std::recursive_mutex mtx_;
 public:
  MultiIndexContainer() = default;
  ~MultiIndexContainer() = default;

  // 追加
  void add(const Ptr& obj, const Id& id, const TypeKey& type) {
   list_.append(obj);
   byId_.insert(id, obj);
   byType_.insert(type, obj);
  }

  void addSafe(const Ptr& obj, const Id& id, const TypeKey& type)
  {

  }

  // ID検索
  Ptr findById(const Id& id) const {
   return byId_.value(id);
  }

  // 型検索
  QList<Ptr> findByType(const TypeKey& type) const {
   return byType_.values(type);
  }

  // 全件取得
  const QVector<Ptr>& all() const { return list_; }

  // 削除
  void removeById(const Id& id) {
   if (auto obj = byId_.take(id)) {
    list_.removeAll(obj);
    byType_.remove(typeid(*obj), obj);
   }
  }
 };

 

};