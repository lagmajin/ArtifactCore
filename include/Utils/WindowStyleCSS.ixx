module;
#include "..\Define\DllExportMacro.hpp"
#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPalette>
#include <QStyleFactory>
#include <QString>

export module Widgets.Utils.CSS;

export namespace ArtifactCore {

enum class DccStylePreset {
  DefaultQt,
  MayaStyle,
  ModoStyle,
  StudioStyle,
  BlenderStyle,
  DaVinciStyle,
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

DccStyleTheme LIBRARY_DLL_API getDCCTheme(DccStylePreset preset);
QPalette LIBRARY_DLL_API buildDCCPalette(const DccStyleTheme& theme);
QString LIBRARY_DLL_API buildDCCStyleSheet(const DccStyleTheme &theme);
DccStyleTheme& LIBRARY_DLL_API currentDCCTheme();
bool LIBRARY_DLL_API loadDCCThemePresetFromFile(const QString& filePath, DccStyleTheme* theme,
                                               QString* errorMessage = nullptr);
void LIBRARY_DLL_API applyDCCTheme(QApplication& app, const DccStyleTheme& theme);

QString LIBRARY_DLL_API themePresetKey(DccStylePreset preset) {
  switch (preset) {
  case DccStylePreset::DefaultQt:
    return QStringLiteral("Light");
  case DccStylePreset::MayaStyle:
    return QStringLiteral("Maya");
  case DccStylePreset::ModoStyle:
    return QStringLiteral("Modo");
  case DccStylePreset::StudioStyle:
    return QStringLiteral("Studio");
  case DccStylePreset::BlenderStyle:
    return QStringLiteral("Blender");
  case DccStylePreset::DaVinciStyle:
    return QStringLiteral("DaVinci");
  case DccStylePreset::_3dsMaxStyle:
    return QStringLiteral("3ds Max");
  case DccStylePreset::NukeStyle:
    return QStringLiteral("Nuke");
  case DccStylePreset::HighContrast:
    return QStringLiteral("High Contrast");
  }
  return QStringLiteral("Studio");
}

QString LIBRARY_DLL_API themePresetLabel(DccStylePreset preset) {
  return themePresetKey(preset);
}

DccStylePreset LIBRARY_DLL_API themePresetFromName(const QString& name) {
  const QString key = name.trimmed();
  if (key.compare(QStringLiteral("High Contrast"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("HighContrast"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::HighContrast;
  }
  if (key.compare(QStringLiteral("Maya"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("MayaStyle"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::MayaStyle;
  }
  if (key.compare(QStringLiteral("Modo"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("ModoStyle"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::ModoStyle;
  }
  if (key.compare(QStringLiteral("Studio"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("StudioStyle"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::StudioStyle;
  }
  if (key.compare(QStringLiteral("Blender"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("BlenderStyle"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::BlenderStyle;
  }
  if (key.compare(QStringLiteral("DaVinci"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("Resolve"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("DaVinciStyle"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::DaVinciStyle;
  }
  if (key.compare(QStringLiteral("3ds Max"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("3dsMax"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("_3dsMaxStyle"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::_3dsMaxStyle;
  }
  if (key.compare(QStringLiteral("Nuke"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("NukeStyle"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::NukeStyle;
  }
  if (key.compare(QStringLiteral("Light"), Qt::CaseInsensitive) == 0 ||
      key.compare(QStringLiteral("DefaultQt"), Qt::CaseInsensitive) == 0) {
    return DccStylePreset::DefaultQt;
  }
  return DccStylePreset::StudioStyle;
}

void LIBRARY_DLL_API applyDCCTheme(QApplication& app, const DccStyleTheme& theme) {
  currentDCCTheme() = theme;
  app.setPalette(buildDCCPalette(theme));
  // Theme application should be palette-driven. Keep style sheets empty so the
  // UI is not split across multiple QSS layers.
  app.setStyleSheet(QString());
}

void LIBRARY_DLL_API applyDCCTheme(QApplication& app, DccStylePreset preset) {
  applyDCCTheme(app, getDCCTheme(preset));
}

void LIBRARY_DLL_API applyDCCTheme(QApplication& app, const QString& themeName) {
  applyDCCTheme(app, themePresetFromName(themeName));
}

bool LIBRARY_DLL_API loadDCCThemePresetFromFile(const QString& filePath, DccStyleTheme* theme,
                                               QString* errorMessage) {
  if (!theme) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Theme output pointer is null.");
    }
    return false;
  }

  const QFileInfo info(filePath);
  if (!info.exists() || !info.isFile()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Theme preset file does not exist: %1").arg(filePath);
    }
    return false;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to open theme preset file: %1").arg(filePath);
    }
    return false;
  }

  const auto doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Theme preset file must contain a JSON object: %1")
                          .arg(filePath);
    }
    return false;
  }

  const QJsonObject root = doc.object();
  DccStyleTheme loaded =
      getDCCTheme(themePresetFromName(root.value(QStringLiteral("base")).toString(
          QStringLiteral("Studio"))));

  const auto applyColor = [&root, &loaded](const char* key, QString DccStyleTheme::*field) {
    const QJsonValue value = root.value(QLatin1String(key));
    if (!value.isString()) {
      return;
    }
    const QString color = value.toString().trimmed();
    if (!QColor(color).isValid()) {
      return;
    }
    loaded.*field = color;
  };

  applyColor("accentColor", &DccStyleTheme::accentColor);
  applyColor("textColor", &DccStyleTheme::textColor);
  applyColor("backgroundColor", &DccStyleTheme::backgroundColor);
  applyColor("secondaryBackgroundColor", &DccStyleTheme::secondaryBackgroundColor);
  applyColor("selectionColor", &DccStyleTheme::selectionColor);
  applyColor("borderColor", &DccStyleTheme::borderColor);
  applyColor("buttonColor", &DccStyleTheme::buttonColor);
  applyColor("buttonHoverColor", &DccStyleTheme::buttonHoverColor);
  applyColor("buttonPressedColor", &DccStyleTheme::buttonPressedColor);

  *theme = loaded;
  return true;
}

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
  case DccStylePreset::StudioStyle:
    theme.accentColor = "#D9A35A";
    theme.textColor = "#E3E6EA";
    theme.backgroundColor = "#1E1E1E";
    theme.secondaryBackgroundColor = "#282828";
    theme.selectionColor = "#3A3A3A";
    theme.borderColor = "#3C3C3C";
    theme.buttonColor = "#2E2E2E";
    theme.buttonHoverColor = "#383838";
    theme.buttonPressedColor = "#252525";
    break;
  case DccStylePreset::BlenderStyle:
    theme.accentColor = "#F5792A";
    theme.textColor = "#E8E8E8";
    theme.backgroundColor = "#26292E";
    theme.secondaryBackgroundColor = "#31353B";
    theme.selectionColor = "#51402A";
    theme.borderColor = "#3D434B";
    theme.buttonColor = "#343A40";
    theme.buttonHoverColor = "#3E454C";
    theme.buttonPressedColor = "#2C3137";
    break;
  case DccStylePreset::DaVinciStyle:
    theme.accentColor = "#4FA8FF";
    theme.textColor = "#E1E7EF";
    theme.backgroundColor = "#111418";
    theme.secondaryBackgroundColor = "#1A1F25";
    theme.selectionColor = "#1B3550";
    theme.borderColor = "#29313A";
    theme.buttonColor = "#1D232A";
    theme.buttonHoverColor = "#27303A";
    theme.buttonPressedColor = "#151B21";
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

QPalette LIBRARY_DLL_API buildDCCPalette(const DccStyleTheme& theme) {
  QPalette pal;
  const QColor accent(theme.accentColor);
  const QColor text(theme.textColor);
  const QColor bg(theme.backgroundColor);
  const QColor panel(theme.secondaryBackgroundColor);
  const QColor selection(theme.selectionColor);
  const QColor border(theme.borderColor);

  pal.setColor(QPalette::Window, bg);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, panel);
  pal.setColor(QPalette::AlternateBase, bg);
  pal.setColor(QPalette::ToolTipBase, panel);
  pal.setColor(QPalette::ToolTipText, text);
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, panel);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::BrightText, QColor("#FFFFFF"));
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
  pal.setColor(QPalette::Link, accent);
  pal.setColor(QPalette::LinkVisited, accent.darker(120));
  pal.setColor(QPalette::PlaceholderText, border.lighter(140));
  return pal;
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

        QDockWidget, QToolBar, QStatusBar {
            background-color: %2;
            color: %1;
            border: none;
        }

        QGroupBox {
            border: 1px solid %6;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 8px;
            background-color: %3;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
            color: %1;
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

        QToolButton {
            background-color: transparent;
            color: %1;
            border: 1px solid transparent;
            border-radius: 2px;
            padding: 3px 6px;
        }
        QToolButton:hover {
            background-color: %8;
            border-color: %6;
        }
        QToolButton:checked {
            background-color: %4;
            border-color: %0;
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

DccStyleTheme& LIBRARY_DLL_API currentDCCTheme() {
  static DccStyleTheme theme = getDCCTheme(DccStylePreset::StudioStyle);
  return theme;
}

QString LIBRARY_DLL_API getDCCStyleSheetPreset(DccStylePreset preset) {
  return buildDCCStyleSheet(getDCCTheme(preset));
}

}; // namespace ArtifactCore
