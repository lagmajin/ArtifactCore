module;
#include <utility>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <opencv2/gapi.hpp>
#include <opencv2/gapi/core.hpp>
#include <opencv2/gapi/cpu/gcpukernel.hpp>
#include <opencv2/gapi/fluid/gfluidbuffer.hpp>
#include <opencv2/gapi/fluid/gfluidkernel.hpp>
#include <opencv2/gapi/imgproc.hpp>
export module BlurGAPI;

namespace ArtifactCore {

G_API_OP(CustomBlur, <cv::GMat(cv::GMat, int)>,
         "custom.blur"){static cv::GMatDesc outMeta(cv::GMatDesc in, int){
    return in; // 入力と同じサイズ・タイプを返す
}
}
;
}
