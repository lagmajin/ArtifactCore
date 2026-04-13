module;
//#include "../../../Define/DllExportMacro.hpp"
#include <QImage>
//#include "../../../../../ArtifactCore/include/Define/DllExportMacro.hpp"

export module OpenCV.FrameInterpolator;

export namespace ArtifactCore {

class  FrameInterpolator {
public:
    // オプティカルフローを使ったピクセル補間
    static QImage interpolateOpticalFlow(const QImage& frame1, const QImage& frame2, float t);

    // 移動速度（ベクトル）から自動計算するモーションブラー
    // velocity = ピクセル単位の移動ベクトル
    // intensity = ブラーの強さ (0〜1など)
    static QImage applyMotionBlur(const QImage& frame, float velocityX, float velocityY, float intensity = 1.0f);
};

}
