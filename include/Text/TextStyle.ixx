module;

#include <QString>

export module Text.Style;

import Utils.String.UniString;
import FloatRGBA;

export namespace ArtifactCore {

export enum class TextHorizontalAlignment {
 Left = 0,
 Center = 1,
 Right = 2,
 Justify = 3
};

export enum class TextVerticalAlignment {
 Top = 0,
 Middle = 1,
 Bottom = 2
};

export struct TextStyle {
 UniString fontFamily = UniString(QStringLiteral("Arial"));
 float fontSize = 60.0f;
 float tracking = 0.0f;
 float leading = -1.0f;
 bool bold = false;
 bool italic = false;
 bool allCaps = false;
 FloatRGBA fillColor = FloatRGBA(1.0f, 1.0f, 1.0f, 1.0f);
};

export struct ParagraphStyle {
 TextHorizontalAlignment horizontalAlignment = TextHorizontalAlignment::Left;
 TextVerticalAlignment verticalAlignment = TextVerticalAlignment::Top;
 float paragraphSpacing = 0.0f;
};

}
