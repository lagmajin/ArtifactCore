module;
#include <cstddef>
#include <cstdint>
#include <functional>
#include <algorithm>
#include <limits>
#include <memory>
#include <type_traits>
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
         * @brief プロセス共有の計算 arena を初回だけ初期化します。
         * maxConcurrency が 1 以上ならその上限を使い、0 以下なら
         * oneTBB の自動 concurrency を使います。初回初期化後は変更しません。
         */
        static int InitializeSharedArena(int maxConcurrency);

        /** @brief 共有 arena の最大同時実行数を返します。 */
        static int SharedArenaConcurrency();

        /**
         * @brief コールバックを `Parallel::For` と同じ共有 arena で同期実行します。
         * 呼び出しが戻るまでに完了し、コールバックの参照先はその間だけ借用されます。
         */
        template<typename Function>
        static void ExecuteInSharedArena(Function&& func) {
            using FunctionType = std::remove_reference_t<Function>;
            auto* context = const_cast<void*>(
                static_cast<const void*>(std::addressof(func)));
            ExecuteInSharedArenaErased(context, [](void* opaque) {
                (*static_cast<FunctionType*>(opaque))();
            });
        }

        // Keep the existing type-erased entry point for callers that already
        // own a std::function; templated callers avoid constructing one.
        static void ExecuteInSharedArena(const std::function<void()>& func);

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
         * callback は複数 worker から同時に呼ばれる場合があるため、
         * タイル外の共有状態は同期するか、読み取り専用にしてください。
         */
        template<typename Function>
        static void ForTiles(int width, int height,
                             int tileWidth, int tileHeight,
                             Function func) {
            ForTilesImpl(width, height, tileWidth, tileHeight,
                         std::move(func), 0);
        }

        /**
         * @brief tile 数に対する grain size を指定して2D領域を並列処理します。
         * @param grainSize 1 task が受け持つ tile index 数の目安。1以上に丸めます。
         */
        template<typename Function>
        static void ForTiles(int width, int height,
                             int tileWidth, int tileHeight,
                             int grainSize, Function func) {
            ForTilesImpl(width, height, tileWidth, tileHeight,
                         std::move(func), std::max(1, grainSize));
        }

    private:
        template<typename Function>
        static void ForTilesImpl(int width, int height,
                                 int tileWidth, int tileHeight,
                                 Function func, int grainSize) {
            if (width <= 0 || height <= 0 || tileWidth <= 0 || tileHeight <= 0) {
                return;
            }

            const int tilesX = width / tileWidth + (width % tileWidth != 0);
            const int tilesY = height / tileHeight + (height % tileHeight != 0);
            const std::size_t tileWidthCount = static_cast<std::size_t>(tilesX);
            const std::size_t tileHeightCount = static_cast<std::size_t>(tilesY);
            if (tileWidthCount >
                std::numeric_limits<std::size_t>::max() / tileHeightCount) {
                for (int tileY = 0; tileY < tilesY; ++tileY) {
                    const int y0 = tileY * tileHeight;
                    const int y1 = y0 + std::min(tileHeight, height - y0);
                    for (int tileX = 0; tileX < tilesX; ++tileX) {
                        const int x0 = tileX * tileWidth;
                        const int x1 = x0 + std::min(tileWidth, width - x0);
                        func(x0, y0, x1, y1);
                    }
                }
                return;
            }
            const std::size_t tileCount = tileWidthCount * tileHeightCount;
            const int pixelWorkItems = SaturatedPixelWorkItems(width, height);

            const auto processTile = [&](std::size_t tileIndex) {
                const int tileX = static_cast<int>(tileIndex % tileWidthCount);
                const int tileY = static_cast<int>(tileIndex / tileWidthCount);
                const int x0 = tileX * tileWidth;
                const int y0 = tileY * tileHeight;
                const int x1 = x0 + std::min(tileWidth, width - x0);
                const int y1 = y0 + std::min(tileHeight, height - y0);
                func(x0, y0, x1, y1);
            };
            if (grainSize > 0) {
                ForSize(0, tileCount,
                        static_cast<std::size_t>(pixelWorkItems),
                        static_cast<std::size_t>(grainSize), processTile);
            } else {
                ForSize(0, tileCount,
                        static_cast<std::size_t>(pixelWorkItems), processTile);
            }
        }

    public:
        /**
         * @brief start から end-1 までの範囲を並列処理します
         * @param start 開始インデックス（包含）
         * @param end 終了インデックス（排他）
         * @param func 実行する関数: void(int index)
         * 並列分岐では複数 index から同じ callable が同時に呼び出されます。
         */
        template<typename Function>
        static void For(int start, int end, Function func) {
            if (start >= end) return;

            constexpr int kParallelRangeThreshold = 64;
            if (static_cast<std::int64_t>(end) -
                    static_cast<std::int64_t>(start) < kParallelRangeThreshold) {
                for (int i = start; i < end; ++i) {
                    func(i);
                }
                return;
            }

            using FunctionType = std::remove_reference_t<Function>;
            auto* context = static_cast<void*>(std::addressof(func));
            ForErased(start, end, context, [](void* opaque, int index) {
                (*static_cast<FunctionType*>(opaque))(index);
            });
        }

        // size_t index ranges avoid narrowing large image/buffer counts to int.
        // The callback follows the same shared-callable concurrency contract
        // as For: worker threads may invoke it simultaneously.
        template<typename Function>
        static void ForSize(std::size_t start, std::size_t end,
                            Function func) {
            if (start >= end) return;

            constexpr std::size_t kParallelRangeThreshold = 64;
            if (end - start < kParallelRangeThreshold) {
                for (std::size_t index = start; index < end; ++index) {
                    func(index);
                }
                return;
            }

            using FunctionType = std::remove_reference_t<Function>;
            auto* context = static_cast<void*>(std::addressof(func));
            ForSizeErased(start, end, context,
                [](void* opaque, std::size_t index) {
                    (*static_cast<FunctionType*>(opaque))(index);
                });
        }

        // Work-estimate overload preserves the same minimum-work policy as
        // For(start, end, workItems) without narrowing either count to int.
        template<typename Function>
        static void ForSize(std::size_t start, std::size_t end,
                            std::size_t workItems, Function func) {
            if (start >= end) return;

            constexpr std::size_t kMinimumParallelRange = 2;
            constexpr std::size_t kParallelWorkThreshold = 4096;
            if (end - start < kMinimumParallelRange ||
                workItems < kParallelWorkThreshold) {
                for (std::size_t index = start; index < end; ++index) {
                    func(index);
                }
                return;
            }

            using FunctionType = std::remove_reference_t<Function>;
            auto* context = static_cast<void*>(std::addressof(func));
            ForSizeErased(start, end, context,
                [](void* opaque, std::size_t index) {
                    (*static_cast<FunctionType*>(opaque))(index);
                });
        }

        // Work-estimate and grain-size overload for size_t ranges. This keeps
        // large tile/index domains parallel without narrowing the range.
        template<typename Function>
        static void ForSize(std::size_t start, std::size_t end,
                            std::size_t workItems, std::size_t grainSize,
                            Function func) {
            if (start >= end) return;

            constexpr std::size_t kMinimumParallelRange = 2;
            constexpr std::size_t kParallelWorkThreshold = 4096;
            if (end - start < kMinimumParallelRange ||
                workItems < kParallelWorkThreshold) {
                for (std::size_t index = start; index < end; ++index) {
                    func(index);
                }
                return;
            }

            using FunctionType = std::remove_reference_t<Function>;
            auto* context = static_cast<void*>(std::addressof(func));
            ForSizeErasedWithGrain(
                start, end, std::max<std::size_t>(1, grainSize), context,
                [](void* opaque, std::size_t index) {
                    (*static_cast<FunctionType*>(opaque))(index);
                });
        }

        /**
         * @brief 反復回数が少なくても、各反復の仕事量を示して並列化できます
         * @param workItems 1反復あたりではなく、範囲全体のおおよその仕事量
         * 並列分岐では複数 index から同じ callable が同時に呼び出されます。
         */
        template<typename Function>
        static void For(int start, int end, int workItems, Function func) {
            if (start >= end) return;

            constexpr int kMinimumParallelRange = 2;
            constexpr int kParallelWorkThreshold = 4096;
            if (static_cast<std::int64_t>(end) -
                    static_cast<std::int64_t>(start) < kMinimumParallelRange ||
                workItems < kParallelWorkThreshold) {
                for (int i = start; i < end; ++i) {
                    func(i);
                }
                return;
            }

            using FunctionType = std::remove_reference_t<Function>;
            auto* context = static_cast<void*>(std::addressof(func));
            ForErased(start, end, context, [](void* opaque, int index) {
                (*static_cast<FunctionType*>(opaque))(index);
            });
        }

        /**
         * @brief 仕事量と1 task あたりの grain size を指定して並列処理します。
         * @param workItems 範囲全体のおおよその仕事量
         * @param grainSize oneTBB が分割する最小 range の目安。1以上に丸めます。
         * 大きい grain は task 数を抑え、小さい grain は負荷の偏りを抑えます。
         * 並列分岐では同じ callable が複数 worker から同時に呼び出されます。
         */
        template<typename Function>
        static void For(int start, int end, int workItems,
                        int grainSize, Function func) {
            if (start >= end) return;

            constexpr int kMinimumParallelRange = 2;
            constexpr int kParallelWorkThreshold = 4096;
            if (static_cast<std::int64_t>(end) -
                    static_cast<std::int64_t>(start) < kMinimumParallelRange ||
                workItems < kParallelWorkThreshold) {
                for (int i = start; i < end; ++i) {
                    func(i);
                }
                return;
            }

            using FunctionType = std::remove_reference_t<Function>;
            auto* context = static_cast<void*>(std::addressof(func));
            ForErasedWithGrain(start, end, std::max(1, grainSize), context,
                [](void* opaque, int index) {
                    (*static_cast<FunctionType*>(opaque))(index);
                });
        }

        // Convert a 2D pixel workload to the int-based scheduler estimate
        // without overflowing for large images. The scheduled index range is
        // still supplied separately by the caller (typically rows or tiles).
        static int SaturatedPixelWorkItems(int width, int height) noexcept {
            if (width <= 0 || height <= 0) return 0;
            return width > std::numeric_limits<int>::max() / height
                ? std::numeric_limits<int>::max()
                : width * height;
        }

        // Estimate a 3D volume workload without overflowing the scheduler's
        // int-based work hint. The scheduled range remains the Z-slice range.
        static int SaturatedVolumeWorkItems(int width, int height,
                                            int depth) noexcept {
            const int sliceWorkItems = SaturatedPixelWorkItems(width, height);
            if (sliceWorkItems == 0 || depth <= 0) return 0;
            return sliceWorkItems > std::numeric_limits<int>::max() / depth
                ? std::numeric_limits<int>::max()
                : sliceWorkItems * depth;
        }

        /** @brief Zスライスごとに独立した3D領域を並列処理します。 */
        template<typename Function>
        static void ForVolumeSlices(int width, int height, int depth,
                                    Function func) {
            if (width <= 0 || height <= 0 || depth <= 0) return;
            For(0, depth, SaturatedVolumeWorkItems(width, height, depth),
                std::move(func));
        }

        /** @brief 1 taskあたりのZスライス数を指定して3D領域を処理します。 */
        template<typename Function>
        static void ForVolumeSlices(int width, int height, int depth,
                                    int grainSize, Function func) {
            if (width <= 0 || height <= 0 || depth <= 0) return;
            For(0, depth, SaturatedVolumeWorkItems(width, height, depth),
                grainSize, std::move(func));
        }

        // Row-oriented image work with a saturating pixel estimate. This keeps
        // large images on the parallel path instead of overflowing the work
        // threshold expression at call sites.
        template<typename Function>
        static void ForPixels(int start, int end, int width, int height,
                              Function func) {
            if (start >= end || height <= 0) return;
            For(start, end, SaturatedPixelWorkItems(width, height),
                std::move(func));
        }

        // Row-oriented image work with an explicit row grain. Use this when
        // profiling shows that the workload benefits from a tuned rows-per-task
        // target instead of the default partitioning.
        template<typename Function>
        static void ForPixels(int start, int end, int width, int height,
                              int grainSize, Function func) {
            if (start >= end || height <= 0) return;
            For(start, end, SaturatedPixelWorkItems(width, height),
                grainSize, std::move(func));
        }

    private:
        using WorkInvoker = void(*)(void*, int);
        using SizeWorkInvoker = void(*)(void*, std::size_t);
        using ArenaInvoker = void(*)(void*);
        // Keep the pre-erasure ABI available while dependent modules from an
        // incremental build are being regenerated after this module changes.
        static void ForErased(int start, int end,
                              const std::function<void(int)>& func);
        static void ForErased(int start, int end, void* context, WorkInvoker invoke);
        static void ForSizeErased(std::size_t start, std::size_t end,
                                  void* context, SizeWorkInvoker invoke);
        static void ForSizeErasedWithGrain(std::size_t start, std::size_t end,
                                           std::size_t grainSize,
                                           void* context,
                                           SizeWorkInvoker invoke);
        static void ForErasedWithGrain(int start, int end, int grainSize,
                                       void* context, WorkInvoker invoke);
        static void ExecuteInSharedArenaErased(void* context, ArenaInvoker invoke);
    };

}
