// ReSharper disable All
module;

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QPointF>
#include <QtCore/QSize>
#include <QtGui/QImage>
#include <QtGui/QColor>
#include <QtGui/QRgb>
#include <QtCore/QMatrix3x3>

#include "../Define/DllExportMacro.hpp"

export module Video.Stabilizer;

import std;
import Frame.Position;

export namespace ArtifactCore {

// 特徴点のトラック
struct FeatureTrack {
    QVector<QPointF> positions;
    int id = -1;
    bool valid = true;
};

// フレームの動きベクトル
struct FrameMotion {
    double x = 0;
    double y = 0;
    double rotation = 0;
    double scale = 1.0;
    
    QPointF center;
    
    QMatrix3x3 toMatrix() const;
    FrameMotion inverted() const;
};

// 特徴点検出のパラメータ
struct FeatureDetectionParams {
    int maxFeatures = 200;
    double qualityLevel = 0.01;
    double minDistance = 10.0;
    int blockSize = 3;
    bool useHarrisDetector = false;
    double k = 0.04;
};

// スタビライザーパラメータ
struct StabilizerParams {
    FeatureDetectionParams featureParams;
    
    int smoothingWindowSize = 30;
    bool stabilizeTranslation = true;
    bool stabilizeRotation = true;
    bool stabilizeScale = false;
    
    double borderFill = 0.0;
    QSize outputSize;
    
    double robustThreshold = 3.0;
    int maxIterations = 100;
    
    bool showFeatures = false;
    bool showMotionVectors = false;
    QColor debugColor = Qt::red;
};

class LIBRARY_DLL_API VideoStabilizer {
public:
    VideoStabilizer();
    ~VideoStabilizer();
    
    void setParams(const StabilizerParams& params);
    const StabilizerParams& params() const { return params_; }
    
    void addFrame(const QImage& frame, FramePosition pos = FramePosition(0));
    void addFrame(const uchar* data, int width, int height, FramePosition pos = FramePosition(0));
    
    bool stabilize();
    
    QImage getStabilizedFrame(int index) const;
    
    QVector<FrameMotion> getFrameMotions() const { return motions_; }
    QVector<FrameMotion> getSmoothedMotions() const { return smoothedMotions_; }
    QVector<FeatureTrack> getFeatureTracks() const { return featureTracks_; }
    
    double getProcessingTime() const { return processingTime_; }
    int getFramesProcessed() const { return processedFrames_; }
    int getFeaturesDetected() const { return totalFeatures_; }
    
    void clear();
    
signals:
    void progressChanged(int current, int total);
    void stabilizationComplete();
    
private:
    QVector<QPointF> detectFeatures(const QImage& frame) const;
    QVector<int> trackFeatures(
        const QImage& prevFrame, 
        const QImage& currFrame,
        const QVector<QPointF>& prevFeatures,
        QVector<QPointF>& currFeatures
    ) const;
    FrameMotion estimateMotion(
        const QVector<QPointF>& prevPoints,
        const QVector<QPointF>& currPoints
    ) const;
    FrameMotion robustEstimateMotion(
        const QVector<QPointF>& prevPoints,
        const QVector<QPointF>& currPoints
    ) const;
    void smoothMotions();
    QImage stabilizeFrame(const QImage& frame, int index) const;
    QImage transformImage(
        const QImage& image,
        const FrameMotion& motion,
        const QSize& outputSize
    ) const;
    QRgb getBorderPixel(const QImage& image, int x, int y) const;
    bool trackFeaturesBetweenFrames();
    void estimateFrameMotions();
    
    StabilizerParams params_;
    QVector<QImage> frames_;
    QVector<FramePosition> framePositions_;
    QVector<FrameMotion> motions_;
    QVector<FrameMotion> smoothedMotions_;
    QVector<FeatureTrack> featureTracks_;
    int processedFrames_ = 0;
    double processingTime_ = 0.0;
    int totalFeatures_ = 0;
};

class LIBRARY_DLL_API LiveStabilizer {
public:
    LiveStabilizer();
    ~LiveStabilizer();
    
    void setParams(const StabilizerParams& params);
    const StabilizerParams& params() const { return params_; }
    void setMaxHistorySize(int size) { maxHistorySize_ = size; }
    int maxHistorySize() const { return maxHistorySize_; }
    
    QImage processFrame(const QImage& frame);
    void reset();
    
signals:
    void stabilizationComplete();
    
private:
    StabilizerParams params_;
    int maxHistorySize_ = 30;
    QVector<QImage> history_;
    QVector<FrameMotion> motionHistory_;
    VideoStabilizer stabilizer_;
    bool initialized_ = false;
};

class LIBRARY_DLL_API BatchStabilizer {
public:
    BatchStabilizer();
    ~BatchStabilizer();
    
    void setParams(const StabilizerParams& params);
    const StabilizerParams& params() const { return params_; }
    void setInputFile(const QString& filePath) { inputFile_ = filePath; }
    QString inputFile() const { return inputFile_; }
    void setOutputFile(const QString& filePath) { outputFile_ = filePath; }
    QString outputFile() const { return outputFile_; }
    
    bool process();
    
signals:
    void progressChanged(int current, int total);
    void stabilizationComplete();
    
private:
    StabilizerParams params_;
    QString inputFile_;
    QString outputFile_;
    int currentFrame_ = 0;
    int totalFrames_ = 0;
};

} // namespace ArtifactCore
