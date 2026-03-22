module;
#include <windows.h>

module Platform.Power;

namespace ArtifactCore
{
    void SystemPower::setPreventSleep(bool prevent)
    {
        if (prevent) {
            // ES_CONTINUOUS: 設定を継続
            // ES_SYSTEM_REQUIRED: システムのスリープを防止
            // ES_AWAYMODE_REQUIRED: ユーザーが離席しても「離席モード」で動作継続（必要なら）
            SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
        } else {
            // 標準状態に戻す
            SetThreadExecutionState(ES_CONTINUOUS);
        }
    }
}
