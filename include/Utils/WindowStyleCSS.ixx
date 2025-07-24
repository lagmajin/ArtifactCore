module;
#include <QString>
#include "..\ArtifactWidgets\include\Define\DllExportMacro.hpp"
export module Widgets.Utils.CSS;

export namespace ArtifactCore
{

 enum class DccStylePreset {
  DefaultQt,  // Qtのデフォルトスタイル
  MayaStyle,
  ModoStyle,
  _3dsMaxStyle, // 3ds Max の場合、数字で始まるのでアンダースコアを追加
  NukeStyle,
  // その他、 HoudiniStyle, BlenderStyle など追加可能
 };

 QString LIBRARY_DLL_API getDCCStyleSheetPreset(DccStylePreset preset)
 {
  QString accentColor;
  QString textColor;
  QString backgroundColor;
  QString secondaryBackgroundColor; // ウィジェットの背景など、少し異なる背景色
  QString selectionColor;           // 選択行や選択テキストの色
  QString borderColor;              // 枠線の色
  QString buttonColor;
  QString buttonHoverColor;
  QString buttonPressedColor;

  // 各プリセットの色定義
  switch (preset) {
  case DccStylePreset::MayaStyle:
   accentColor = "#88C0D0";        // 北極星のような青
   textColor = "#C0C0C0";          // 明るいグレーのテキスト
   backgroundColor = "#323232";    // Mayaのような濃いグレーの背景
   secondaryBackgroundColor = "#404040"; // 少し明るいグレー
   selectionColor = "#4A6E8A";     // 選択色
   borderColor = "#505050";        // 枠線
   buttonColor = "#505050";
   buttonHoverColor = "#5A5A5A";
   buttonPressedColor = "#404040";
   break;
  case DccStylePreset::ModoStyle:
   accentColor = "#F5933C";        // Modoのようなオレンジ
   textColor = "#E0E0E0";          // 明るいテキスト
   backgroundColor = "#2E2E2E";    // Modoのような濃いダークグレー
   secondaryBackgroundColor = "#3A3A3A";
   selectionColor = "#5C3E20";     // 暗めのオレンジの選択
   borderColor = "#454545";
   buttonColor = "#404040";
   buttonHoverColor = "#4A4A4A";
   buttonPressedColor = "#3A3A3A";
   break;
  case DccStylePreset::_3dsMaxStyle:
   accentColor = "#FFC200";        // 3ds Maxのような黄色オレンジ
   textColor = "#C0C0C0";
   backgroundColor = "#353535";
   secondaryBackgroundColor = "#454545";
   selectionColor = "#504020";
   borderColor = "#606060";
   buttonColor = "#505050";
   buttonHoverColor = "#555555";
   buttonPressedColor = "#454545";
   break;
  case DccStylePreset::NukeStyle:
   accentColor = "#52C0FE";        // Nukeのような明るいシアン
   textColor = "#C0C0C0";
   backgroundColor = "#2C2C2C";    // Nukeのような非常に暗いグレー
   secondaryBackgroundColor = "#383838";
   selectionColor = "#407080";     // 暗めのシアンの選択
   borderColor = "#404040";
   buttonColor = "#383838";
   buttonHoverColor = "#404040";
   buttonPressedColor = "#303030";
   break;
  case DccStylePreset::DefaultQt: // Fallthrough to a sane default
  default:
   accentColor = "#0078D7";
   textColor = "#333333";
   backgroundColor = "#F0F0F0";
   secondaryBackgroundColor = "#FFFFFF";
   selectionColor = "#B4D5FF";
   borderColor = "#CCCCCC";
   buttonColor = "#F0F0F0";
   buttonHoverColor = "#E0E0E0";
   buttonPressedColor = "#D0D0D0";
   break;
  }

  // ここに共通のスタイルルールを定義し、上記の変数を埋め込む
  QString styleSheet = QString(R"(
        /***** Global Styles *****/
        * {
            font-family: "Segoe UI", "Meiryo UI", sans-serif;
            font-size: 9pt; /* DCCツールは小さめのフォントが多い */
            color: %1;
        }

        /* 全体的な背景色 */
        QMainWindow, QWidget, QDialog {
            background-color: %2;
            border: none;
        }

        /***** QPushButtob *****/
        QPushButton {
            background-color: %7;
            color: %1; /* テキストカラー */
            border: 1px solid %6; /* 枠線色 */
            border-radius: 2px; /* 少し角丸 */
            padding: 4px 10px;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: %8; /* ホバー色 */
        }
        QPushButton:pressed {
            background-color: %9; /* 押された時の色 */
            border: 1px solid %0; /* アクセントカラーの枠線 */
        }
        QPushButton:disabled {
            background-color: %6; /* 枠線色と同じくらい暗く */
            color: #808080;
        }

        /***** QLineEdit, QTextEdit, QSpinBox, QComboBox *****/
        QLineEdit, QTextEdit, QSpinBox, QComboBox {
            border: 1px solid %6; /* 枠線色 */
            border-radius: 2px;
            padding: 2px;
            background-color: %3; /* 少し明るい背景 */
            selection-background-color: %0; /* アクセントカラー */
            selection-color: white; /* 白い選択テキスト */
        }
        QLineEdit:focus, QTextEdit:focus, QSpinBox:focus, QComboBox:focus {
            border: 1px solid %0; /* フォーカス時にアクセントカラー */
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 15px;
            border-left-width: 1px;
            border-left-color: %6;
            border-left-style: solid;
            border-top-right-radius: 2px;
            border-bottom-right-radius: 2px;
            background-color: %3;
        }
        QComboBox::down-arrow {
            image: url(:/qss_icons/rc/arrow_down.png); /* 適切なアイコンパス */
        }

        /***** QSlider *****/
        QSlider::groove:horizontal {
            border: 1px solid %6;
            height: 4px;
            background: %3;
            margin: 2px 0;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: %0; /* アクセントカラー */
            border: 1px solid %0;
            width: 12px;
            height: 12px;
            margin: -4px 0; /* ハンドルの中心を溝に合わせる */
            border-radius: 6px; /* 円形 */
        }

        /***** QCheckBox, QRadioButton *****/
        QCheckBox::indicator, QRadioButton::indicator {
            width: 14px;
            height: 14px;
        }
        QCheckBox::indicator:unchecked {
            image: url(:/qss_icons/rc/checkbox_unchecked.png);
        }
        QCheckBox::indicator:checked {
            image: url(:/qss_icons/rc/checkbox_checked.png);
        }
        QRadioButton::indicator:unchecked {
            image: url(:/qss_icons/rc/radio_unchecked.png);
        }
        QRadioButton::indicator:checked {
            image: url(:/qss_icons/rc/radio_checked.png);
        }

        /***** QTabBar / QTabWidget *****/
        QTabWidget::pane {
            border-top: 1px solid %6;
        }
        QTabBar::tab {
            background: %3;
            border: 1px solid %6;
            border-bottom-color: %6;
            border-top-left-radius: 2px;
            border-top-right-radius: 2px;
            padding: 4px 10px;
        }
        QTabBar::tab:selected {
            background: %2; /* ウィンドウの背景色と同じ */
            border-bottom-color: %2;
            color: %0; /* 選択タブのテキスト色 */
        }
        QTabBar::tab:hover {
            background: %8; /* ボタンのホバー色と同じ */
        }

        /***** QHeaderView (Table/Tree Headers) *****/
        QHeaderView::section {
            background-color: %3;
            color: %1;
            padding: 4px;
            border: 1px solid %6;
            border-bottom: 1px solid %6;
        }

        /***** QTreeView, QTableView, QListView *****/
        QTreeView, QTableView, QListView {
            background-color: %2;
            alternate-background-color: %3; /* 縞模様の背景 */
            border: 1px solid %6;
            selection-background-color: %4; /* 選択色 */
            selection-color: %1; /* 選択テキスト色 */
            show-decoration-selected: 1; /* アイテムが選択されたときにテキストを装飾 */
        }
        QTreeView::item:hover, QListView::item:hover {
            background-color: %8;
        }
        QTreeView::item:selected, QListView::item:selected {
            color: %1; /* 選択されたアイテムのテキスト色 */
        }

        /***** QToolTip *****/
        QToolTip {
            background-color: %7; /* ボタンの背景色と同じ */
            color: %1;
            border: 1px solid %6;
            padding: 2px;
            border-radius: 2px;
        }

        /***** QScrollBar *****/
        QScrollBar:vertical {
            border: 1px solid %6;
            background: %3;
            width: 12px;
            margin: 18px 0 18px 0;
            border-radius: 2px;
        }
        QScrollBar::handle:vertical {
            background: %7; /* ボタンの色と同じ */
            min-height: 20px;
            border-radius: 2px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            border: 1px solid %6;
            background: %3;
            height: 18px;
            subcontrol-origin: margin;
        }
        QScrollBar::add-line:vertical {
            subcontrol-position: bottom;
            border-bottom-left-radius: 2px;
            border-bottom-right-radius: 2px;
        }
        QScrollBar::sub-line:vertical {
            subcontrol-position: top;
            border-top-left-radius: 2px;
            border-top-right-radius: 2px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
        QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {
            width: 10px;
            height: 10px;
        }
        QScrollBar::up-arrow:vertical {
            image: url(:/qss_icons/rc/arrow_up.png);
        }
        QScrollBar::down-arrow:vertical {
            image: url(:/qss_icons/rc/arrow_down.png);
        }
        
        QScrollBar:horizontal {
            border: 1px solid %6;
            background: %3;
            height: 12px;
            margin: 0 18px 0 18px;
            border-radius: 2px;
        }
        QScrollBar::handle:horizontal {
            background: %7;
            min-width: 20px;
            border-radius: 2px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            border: 1px solid %6;
            background: %3;
            width: 18px;
            subcontrol-origin: margin;
        }
        QScrollBar::add-line:horizontal {
            subcontrol-position: right;
            border-top-right-radius: 2px;
            border-bottom-right-radius: 2px;
        }
        QScrollBar::sub-line:horizontal {
            subcontrol-position: left;
            border-top-left-radius: 2px;
            border-bottom-left-radius: 2px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }
        QScrollBar::left-arrow:horizontal, QScrollBar::right-arrow:horizontal {
            width: 10px;
            height: 10px;
        }
        QScrollBar::left-arrow:horizontal {
            image: url(:/qss_icons/rc/arrow_left.png);
        }
        QScrollBar::right-arrow:horizontal {
            image: url(:/qss_icons/rc/arrow_right.png);
        }

    )").arg(accentColor, textColor, backgroundColor, secondaryBackgroundColor, selectionColor, borderColor,
 buttonColor, buttonHoverColor, buttonPressedColor); // %n で順番に埋め込む

  return styleSheet;
 }









};