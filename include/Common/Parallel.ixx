module;
#include <utility>
#include <vector>
#include <functional>
#include <future>
#include <algorithm>
#include "../Define/DllExportMacro.hpp"

export module Core.Parallel;

import Core.ThreadPool;

export namespace ArtifactCore {

    /**
     * @brief スレッドプールを活用した高速な並列 for ループ
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
            
            int total = end - start;
            int numThreads = std::thread::hardware_concurrency();
            if (numThreads <= 0) numThreads = 1;
            
            // タスクが少なすぎる場合はシングルスレッドで処理
            if (total < numThreads * 4) {
                for (int i = start; i < end; ++i) {
                    func(i);
                }
                return;
            }

            int chunkSize = (total + numThreads - 1) / numThreads;
            
            std::vector<std::future<void>> futures;
            futures.reserve(numThreads);
            
            auto& pool = ThreadPool::globalInstance();
            
            for (int i = 0; i < numThreads; ++i) {
                int chunkStart = start + i * chunkSize;
                int chunkEnd = std::min(chunkStart + chunkSize, end);
                
                if (chunkStart >= chunkEnd) break;
                
                futures.push_back(pool.enqueue([chunkStart, chunkEnd, &func]() {
                    for (int j = chunkStart; j < chunkEnd; ++j) {
                        func(j);
                    }
                }));
            }
            
            // 全スレッドの終了を待機
            for (auto& f : futures) {
                f.wait();
            }
        }
    };

}
