module;
#include <cstddef>
#include <functional>
#include <algorithm>
#include <utility>
#include "../Define/DllExportMacro.hpp"

export module Core.Parallel;

export namespace ArtifactCore {

        /**
         * @brief 標準の並列アルゴリズムを使った高速な並列 for ループ
         * 画像の各行（Y座標）ごとの処理などを大幅に加速させます。
         */
    class LIBRARY_DLL_API Parallel {
    public:
        /**
         * @brief 画像などの2D領域をタイル単位で並列処理します。
         * @param width  対象領域の幅
         * @param height 対象領域の高さ
         * @param tileWidth タイル幅（1以上）
         * @param tileHeight タイル高さ（1以上）
         * @param func [x0, y0, x1, y1) のタイルを処理する関数
         *
         * 各タイルは重ならないため、出力をタイル内だけで完結させる
         * ピクセル処理に安全に使用できます。
         */
        template<typename Function>
        static void ForTiles(int width, int height,
                             int tileWidth, int tileHeight,
                             Function func) {
            if (width <= 0 || height <= 0 || tileWidth <= 0 || tileHeight <= 0) {
                return;
            }

            const int tilesX = (width + tileWidth - 1) / tileWidth;
            const int tilesY = (height + tileHeight - 1) / tileHeight;
            const int tileCount = tilesX * tilesY;

            For(0, tileCount, tileCount, [&](int tileIndex) {
                const int tileX = tileIndex % tilesX;
                const int tileY = tileIndex / tilesX;
                const int x0 = tileX * tileWidth;
                const int y0 = tileY * tileHeight;
                const int x1 = std::min(width, x0 + tileWidth);
                const int y1 = std::min(height, y0 + tileHeight);
                func(x0, y0, x1, y1);
            });
        }

        /**
         * @brief start から end-1 までの範囲を並列処理します
         * @param start 開始インデックス（包含）
         * @param end 終了インデックス（排他）
         * @param func 実行する関数: void(int index)
         */
        template<typename Function>
        static void For(int start, int end, Function func) {
            if (start >= end) return;

            constexpr int kParallelRangeThreshold = 64;
            if (end - start < kParallelRangeThreshold) {
                for (int i = start; i < end; ++i) {
                    func(i);
                }
                return;
            }

            ForErased(start, end, std::function<void(int)>(std::forward<Function>(func)));
        }

        /**
         * @brief 反復回数が少なくても、各反復の仕事量を示して並列化できます
         * @param workItems 1反復あたりではなく、範囲全体のおおよその仕事量
         */
        template<typename Function>
        static void For(int start, int end, int workItems, Function func) {
            if (start >= end) return;

            constexpr int kParallelRangeThreshold = 64;
            constexpr int kParallelWorkThreshold = 4096;
            if (end - start < kParallelRangeThreshold || workItems < kParallelWorkThreshold) {
                for (int i = start; i < end; ++i) {
                    func(i);
                }
                return;
            }

            ForErased(start, end, std::function<void(int)>(std::forward<Function>(func)));
        }

    private:
        static void ForErased(int start, int end, const std::function<void(int)>& func);
    };

}
