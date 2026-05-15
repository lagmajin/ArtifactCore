module;
#include <utility>
#include <windows.h>
#include "../Define/DllExportMacro.hpp"

export module Platform.Power;

export namespace ArtifactCore
{
    /**
     * @brief システムのスリープ抑制を管理するクラス
     */
    class LIBRARY_DLL_API SystemPower
    {
    public:
        /**
         * @brief スリープを抑制するかどうかを設定する
         * @param prevent trueで抑制（スリープしない）、falseで抑制解除
         */
        static void setPreventSleep(bool prevent);

        /**
         * @brief OS 標準の通知音を鳴らす
         */
        static void playNotificationSound();
    };
}
