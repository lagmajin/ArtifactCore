module;
#include <utility>
#include <vector>
#include <memory>
#include "../Define/DllExportMacro.hpp"
#include <QImage>

export module Analyze.SmartPalette;

import Color.Float;
import Image.Raw;

export namespace ArtifactCore {

    /**
     * @brief 画像から特徴的な色（パレット）を抽出するクラス
     */
    class LIBRARY_DLL_API SmartPaletteAnalyzer {
    public:
        /**
         * @brief RawImageから主要な色を抽出します
         */
        static std::vector<FloatColor> extractPalette(const RawImage& image, int colorCount = 5);

        /**
         * @brief QImageから主要な色を抽出します
         */
        static std::vector<FloatColor> extractPalette(const QImage& image, int colorCount = 5);
    };

}
