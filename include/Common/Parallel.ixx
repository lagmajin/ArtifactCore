module;
#include <cstddef>
#include <functional>
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
