module;
#include <QFlags>
#include <QKeyEvent>
export module InputEvent;

export namespace ArtifactCore {

 struct InputEvent
 {
  enum class Type {
   None,
   KeyDown,
   KeyUp,
   MouseDown,
   MouseUp,
   MouseMove,
   MouseWheel,
   TouchBegin,
   TouchMove,
   TouchEnd
  };

  enum class ModifierKey {
   None = 0,
   LShift = 1 << 0,
   RShift = 1 << 1,
   LCtrl = 1 << 2,
   RCtrl = 1 << 3,
   LAlt = 1 << 4,
   RAlt = 1 << 5,
   LMeta = 1 << 6,
   RMeta = 1 << 7, // Cmd / Winキー
  };

  Q_DECLARE_FLAGS(Modifiers, ModifierKey)

   Type type = Type::None;

  // キーボード
  int keyCode = 0;            // Qt::Key
  Modifiers modifiers;        // 左右区別つき
  bool isAutoRepeat = false;  // 長押し
  qint64 timestamp = 0;

  // マウス
  int button = 0;
  QPointF position;
  QPointF delta;
  float wheelDelta = 0.f;
 };
 Q_DECLARE_OPERATORS_FOR_FLAGS(InputEvent::Modifiers)

};