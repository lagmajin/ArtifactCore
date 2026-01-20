module;

#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Frame.Range;

import std;

export namespace ArtifactCore {

 class FramePosition;
 class FrameOffset;
 class FrameRate;

 // フレーム範囲を表すクラス
 // 動画編集におけるタイムライン上の範囲、ワークエリア、レイヤーの持続時間などを表現
 class LIBRARY_DLL_API FrameRange {
 private:
  class Impl;
  Impl* impl_;
  
 public:
  // コンストラクタ
  FrameRange();
  FrameRange(int64_t start, int64_t end);
  FrameRange(const FramePosition& start, const FramePosition& end);
  FrameRange(const FrameRange& other);
  FrameRange(FrameRange&& other) noexcept;
  ~FrameRange();

  // 代入演算子
  FrameRange& operator=(const FrameRange& other);
  FrameRange& operator=(FrameRange&& other) noexcept;

  // 範囲設定
  void setStart(int64_t start);
  void setEnd(int64_t end);
  void setRange(int64_t start, int64_t end);
  void setDuration(int64_t duration);  // 開始点は変えずに長さを設定

  // 範囲取得
  int64_t start() const;
  int64_t end() const;
  int64_t duration() const;  // length() のエイリアス
  int64_t length() const;
  
  FramePosition startPosition() const;
  FramePosition endPosition() const;

  // 検証
  bool isValid() const;  // start <= end
  bool isEmpty() const;  // start == end
  bool isInfinite() const;

  // 範囲チェック
  bool contains(int64_t frame) const;
  bool contains(const FramePosition& position) const;
  bool contains(const FrameRange& other) const;  // 完全に包含するか
  
  bool overlaps(const FrameRange& other) const;  // 重なりがあるか
  bool touches(const FrameRange& other) const;   // 接触しているか（重なりまたは隣接）

  // 範囲操作
  void expand(int64_t frames);  // 両端を拡張
  void expandStart(int64_t frames);
  void expandEnd(int64_t frames);
  
  void shrink(int64_t frames);  // 両端を縮小
  void shrinkStart(int64_t frames);
  void shrinkEnd(int64_t frames);
  
  void shift(int64_t frames);  // 範囲全体を移動
  void shift(const FrameOffset& offset);
  
  FrameRange shifted(int64_t frames) const;  // 移動したコピーを返す
  FrameRange expanded(int64_t frames) const;  // 拡張したコピーを返す
  FrameRange shrinked(int64_t frames) const;  // 縮小したコピーを返す

  // 範囲演算
  FrameRange united(const FrameRange& other) const;      // 和集合（両方を含む範囲）
  FrameRange intersected(const FrameRange& other) const; // 積集合（重なり部分）
  bool intersects(const FrameRange& other, FrameRange& result) const;  // 積集合が存在するか

  // クリッピング
  void clip(const FrameRange& bounds);  // 指定範囲内に制限
  FrameRange clipped(const FrameRange& bounds) const;
  
  int64_t clampFrame(int64_t frame) const;  // フレームを範囲内に丸める
  FramePosition clampPosition(const FramePosition& position) const;

  // イテレーション
  class Iterator {
  private:
   int64_t current_;
   int64_t endValue_;
  public:
   Iterator(int64_t current, int64_t endValue) : current_(current), endValue_(endValue) {}
   int64_t operator*() const { return current_; }
   Iterator& operator++() { ++current_; return *this; }
   bool operator!=(const Iterator& other) const { return current_ != other.current_; }
  };

  Iterator beginIterator() const;
  Iterator endIterator() const;

  // ユーティリティ
  FrameRange normalized() const;  // start > end の場合、入れ替えて正規化
  void normalize();

  std::vector<int64_t> frames() const;  // 範囲内の全フレーム番号を取得
  std::vector<int64_t> uniformSample(int count) const;  // 均等にサンプリング

  // 時間変換
  double durationSeconds(double fps) const;
  double durationSeconds(const FrameRate& rate) const;
  
  QString toTimecode(double fps) const;  // "00:00:10:00" - "00:00:20:00"
  QString toTimecode(const FrameRate& rate) const;

  // シリアライズ
  QJsonObject toJson() const;
  static FrameRange fromJson(const QJsonObject& json);
  
  QString toString() const;  // "[100, 200]" 形式
  static FrameRange fromString(const QString& str);

  // 比較演算子
  bool operator==(const FrameRange& other) const;
  bool operator!=(const FrameRange& other) const;
  bool operator<(const FrameRange& other) const;   // start で比較
  bool operator<=(const FrameRange& other) const;
  bool operator>(const FrameRange& other) const;
  bool operator>=(const FrameRange& other) const;

  // 特殊な範囲
  static FrameRange invalid();     // 無効な範囲
  static FrameRange infinite();    // 無限範囲
  static FrameRange zero();        // 長さ0の範囲
  static FrameRange fromDuration(int64_t start, int64_t duration);
 };

 // エイリアス
 using WorkArea = FrameRange;      // タイムラインのワークエリア
 using PlaybackRange = FrameRange; // 再生範囲
 using LayerRange = FrameRange;    // レイヤーの持続時間

};
