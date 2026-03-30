module;
#include "..\Define\DllExportMacro.hpp"
#include <QString>

export module Widgets.Utils.CSS;

export namespace ArtifactCore {

enum class DccStylePreset {
  DefaultQt,
  MayaStyle,
  ModoStyle,
  _3dsMaxStyle,
  NukeStyle,
  HighContrast,
};

struct DccStyleTheme {
  QString accentColor;
  QString textColor;
  QString backgroundColor;
  QString secondaryBackgroundColor;
  QString selectionColor;
  QString borderColor;
  QString buttonColor;
  QString buttonHoverColor;
  QString buttonPressedColor;
};

DccStyleTheme LIBRARY_DLL_API getDCCTheme(DccStylePreset preset) {
  DccStyleTheme theme{};
  switch (preset) {
  case DccStylePreset::MayaStyle:
    theme.accentColor = "#88C0D0";
    theme.textColor = "#C0C0C0";
    theme.backgroundColor = "#323232";
    theme.secondaryBackgroundColor = "#404040";
    theme.selectionColor = "#4A6E8A";
    theme.borderColor = "#505050";
    theme.buttonColor = "#505050";
    theme.buttonHoverColor = "#5A5A5A";
    theme.buttonPressedColor = "#404040";
    break;
  case DccStylePreset::ModoStyle:
    theme.accentColor = "#F5933C";
    theme.textColor = "#E0E0E0";
    theme.backgroundColor = "#2E2E2E";
    theme.secondaryBackgroundColor = "#3A3A3A";
    theme.selectionColor = "#5C3E20";
    theme.borderColor = "#454545";
    theme.buttonColor = "#404040";
    theme.buttonHoverColor = "#4A4A4A";
    theme.buttonPressedColor = "#3A3A3A";
    break;
  case DccStylePreset::_3dsMaxStyle:
    theme.accentColor = "#FFC200";
    theme.textColor = "#C0C0C0";
    theme.backgroundColor = "#353535";
    theme.secondaryBackgroundColor = "#454545";
    theme.selectionColor = "#504020";
    theme.borderColor = "#606060";
    theme.buttonColor = "#505050";
    theme.buttonHoverColor = "#555555";
    theme.buttonPressedColor = "#454545";
    break;
  case DccStylePreset::NukeStyle:
    theme.accentColor = "#52C0FE";
    theme.textColor = "#C0C0C0";
    theme.backgroundColor = "#2C2C2C";
    theme.secondaryBackgroundColor = "#383838";
    theme.selectionColor = "#407080";
    theme.borderColor = "#404040";
    theme.buttonColor = "#383838";
    theme.buttonHoverColor = "#404040";
    theme.buttonPressedColor = "#303030";
    break;
  case DccStylePreset::HighContrast:
    theme.accentColor = "#0000FF";     // 高コントラストのアクセント（青）
    theme.textColor = "#000000";       // 黒文字
    theme.backgroundColor = "#FFFFFF"; // 白背景
    theme.secondaryBackgroundColor = "#F0F0F0"; // 薄い灰色
    theme.selectionColor = "#FFFF00";           // 黄色選択
    theme.borderColor = "#000000";              // 黒境界線
    theme.buttonColor = "#FFFFFF";              // 白ボタン
    theme.buttonHoverColor = "#E0E0E0";         // 薄い灰色ホバー
    theme.buttonPressedColor = "#C0C0C0";       // 灰色押下
    break;
  case DccStylePreset::DefaultQt:
  default:
    theme.accentColor = "#0078D7";
    theme.textColor = "#333333";
    theme.backgroundColor = "#F0F0F0";
    theme.secondaryBackgroundColor = "#FFFFFF";
    theme.selectionColor = "#B4D5FF";
    theme.borderColor = "#CCCCCC";
    theme.buttonColor = "#F0F0F0";
    theme.buttonHoverColor = "#E0E0E0";
    theme.buttonPressedColor = "#D0D0D0";
    break;
  }
  return theme;
}

QString LIBRARY_DLL_API buildDCCStyleSheet(const DccStyleTheme &theme) {
  return QString(R"(
        * {
            font-family: "Segoe UI", "Meiryo UI", sans-serif;
            font-size: 9pt;
            color: %1;
        }

        QMainWindow, QWidget, QDialog {
            background-color: %2;
            border: none;
        }

        QPushButton {
            background-color: %7;
            color: %1;
            border: 1px solid %6;
            border-radius: 2px;
            padding: 4px 10px;
            min-height: 20px;
        }
        QPushButton:hover { background-color: %8; }
        QPushButton:pressed {
            background-color: %9;
            border: 1px solid %0;
        }
        QPushButton:disabled {
            background-color: %6;
            color: #808080;
        }

        QLineEdit, QTextEdit, QPlainTextEdit, QAbstractSpinBox, QComboBox {
            border: 1px solid %6;
            border-radius: 2px;
            padding: 2px 4px;
            background-color: %3;
            selection-background-color: %0;
            selection-color: white;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QAbstractSpinBox:focus, QComboBox:focus {
            border: 1px solid %0;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 15px;
            border-left: 1px solid %6;
            background-color: %3;
        }

        QMenuBar {
            background-color: %2;
            color: %1;
        }
        QMenuBar::item {
            padding: 4px 10px;
            background: transparent;
        }
        QMenuBar::item:selected {
            background-color: %8;
        }
        QMenu {
            background-color: %3;
            color: %1;
            border: 1px solid %6;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 28px 6px 28px;
            background-color: transparent;
        }
        QMenu::item:selected {
            background-color: %8;
        }
        QMenu::item:disabled {
            color: rgba(170, 170, 170, 100);
            background-color: %3;
        }
        QMenu::item:disabled:selected {
            color: rgba(255, 255, 255, 170);
            background-color: %6;
        }

        QSlider::groove:horizontal {
            border: 1px solid %6;
            height: 4px;
            background: %3;
            margin: 2px 0;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: %0;
            border: 1px solid %0;
            width: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }

        QTabWidget::pane { border-top: 1px solid %6; }
        QTabBar::tab {
            background: %3;
            border: 1px solid %6;
            border-bottom-color: %6;
            border-top-left-radius: 2px;
            border-top-right-radius: 2px;
            padding: 4px 10px;
        }
        QTabBar::tab:selected {
            background: %2;
            border-bottom-color: %2;
            color: %0;
        }
        QTabBar::tab:hover { background: %8; }

        QHeaderView::section {
            background-color: %3;
            color: %1;
            padding: 4px;
            border: 1px solid %6;
        }

        QTreeView, QTableView, QListView {
            background-color: %2;
            alternate-background-color: %3;
            border: 1px solid %6;
            selection-background-color: %4;
            selection-color: %1;
            show-decoration-selected: 1;
        }
        QTreeView::item:hover, QListView::item:hover { background-color: %8; }
        QTreeView::item:selected, QListView::item:selected { color: %1; }

        QToolTip {
            background-color: %7;
            color: %1;
            border: 1px solid %6;
            padding: 2px;
            border-radius: 2px;
        }

        QScrollBar:vertical {
            border: 1px solid %6;
            background: %3;
            width: 10px;
            margin: 0;
            border-radius: 2px;
        }
        QScrollBar::handle:vertical {
            background: %7;
            min-height: 20px;
            border-radius: 2px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

        QScrollBar:horizontal {
            border: 1px solid %6;
            background: %3;
            height: 10px;
            margin: 0;
            border-radius: 2px;
        }
        QScrollBar::handle:horizontal {
            background: %7;
            min-width: 20px;
            border-radius: 2px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }
    )")
      .arg(theme.accentColor, theme.textColor, theme.backgroundColor,
           theme.secondaryBackgroundColor, theme.selectionColor,
           theme.borderColor, theme.buttonColor, theme.buttonHoverColor,
           theme.buttonPressedColor);
}

QString LIBRARY_DLL_API getDCCStyleSheetPreset(DccStylePreset preset) {
  return buildDCCStyleSheet(getDCCTheme(preset));
}

}; // namespace ArtifactCore
