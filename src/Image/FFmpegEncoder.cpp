


import FFMpegEncoder;

#include <QtCore/QFile>

extern "c" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace ArtifactCore {

 FFMpegEncoder::FFMpegEncoder()
 {

 }

 FFMpegEncoder::~FFMpegEncoder()
 {

 }
 void FFMpegEncoder::open(const QFile& file)
 {



 }


 void FFMpegEncoder::close()
 {

 }


}