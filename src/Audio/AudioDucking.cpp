module;

#include <cmath>
#include <QDateTime>

export module Audio.Ducking;

import Audio.Ducking;

namespace ArtifactCore {

    // コンストラクタ
    AudioDucking::AudioDucking(QObject* parent)
        : QObject(parent)
    {
    }

    // デストラクタ
    AudioDucking::~AudioDucking() = default;

    // ダッキングタイプを設定
    void AudioDucking::setDuckingType(DuckingType type)
    {
        settings_.type = type;
    }

    // ダッキング設定整体を設定
    void AudioDucking::setSettings(const DuckingSettings& settings)
    {
        settings_ = settings;
    }

    // プレゼンス検出タイプを設定
    void AudioDucking::setPresenceDetection(PresenceDetection detection)
    {
        settings_.detection = detection;
    }

    // ダッキングレベルを設定（dB）
    void AudioDucking::setDuckLevel(float levelDb)
    {
        settings_.duckLevel = levelDb;
    }

    // しきい値を設定（dB）
    void AudioDucking::setThreshold(float thresholdDb)
    {
        settings_.threshold = thresholdDb;
    }

    // アタックタイムを設定（ミリ秒）
    void AudioDucking::setAttackTime(int ms)
    {
        settings_.attackTime = ms;
    }

    // リリースタイムを設定（ミリ秒）
    void AudioDucking::setReleaseTime(int ms)
    {
        settings_.releaseTime = ms;
    }

    // 有効/無効を設定
    void AudioDucking::setEnabled(bool enabled)
    {
        settings_.enabled = enabled;
        
        if (!enabled) {
            // ダッキング無効時は元の音量に戻す
            isDucking_ = false;
            currentDuckAmount_ = 0.0f;
            envelope_ = 0.0f;
        }
    }

    // トラックを追加
    void AudioDucking::addTrack(int trackId, const QString& name)
    {
        tracks_[trackId] = name;
    }

    // トラックを削除
    void AudioDucking::removeTrack(int trackId)
    {
        tracks_.remove(trackId);
        duckingTargets_.remove(trackId);
        
        if (mainSourceTrackId_ == trackId) {
            mainSourceTrackId_ = -1;
        }
    }

    // 全てのトラックをクリア
    void AudioDucking::clearTracks()
    {
        tracks_.clear();
        duckingTargets_.clear();
        mainSourceTrackId_ = -1;
    }

    // メインソーストラックを設定
    void AudioDucking::setMainSourceTrack(int trackId)
    {
        mainSourceTrackId_ = trackId;
    }

    // ダッキング対象トラックを追加
    void AudioDucking::addDuckingTarget(int trackId)
    {
        duckingTargets_.insert(trackId);
    }

    // ダッキング対象トラックを削除
    void AudioDucking::removeDuckingTarget(int trackId)
    {
        duckingTargets_.remove(trackId);
    }

    // サンプルを処理（単一トラック）
    DuckingResult AudioDucking::processTrack(int trackId, const float* samples, size_t sampleCount)
    {
        DuckingResult result;
        
        if (!settings_.enabled) {
            result.outputGain = 1.0f;
            return result;
        }

        // プレゼンスを検出
        float presence = detectPresence(samples, sampleCount);
        float presenceDb = linearToDb(presence);
        
        // しきい値チェック
        bool aboveThreshold = checkThreshold(presenceDb);
        
        // 目标ゲインを計算
        float targetGain = 1.0f;
        
        if (aboveThreshold && duckingTargets_.contains(trackId)) {
            // ダッキングが必要
            float reductionDb = presenceDb - settings_.threshold;
            float targetDb = settings_.duckLevel;
            float duckAmount = (targetDb - reductionDb) * settings_.ratio;
            
            targetGain = dbToLinear(duckAmount);
            result.isDucking = true;
            result.duckingAmount = duckAmount;
            
            if (!isDucking_) {
                isDucking_ = true;
                emit duckingStarted(trackId);
                emit duckingStateChanged(true, currentDuckAmount_);
            }
        } else {
            // ダッキング不要
            targetGain = 1.0f;
            result.isDucking = false;
            result.duckingAmount = 0.0f;
            
            if (isDucking_) {
                isDucking_ = false;
                emit duckingEnded(trackId);
                emit duckingStateChanged(false, 0.0f);
            }
        }

        // エンベロープを適用（アタック/リリース）
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        bool attacking = targetGain < envelope_;
        
        float envelopeTime = attacking ? settings_.attackTime : settings_.releaseTime;
        float envelopeDelta = static_cast<float>(currentTime - lastAttackTime_) / envelopeTime;
        
        envelope_ = applyEnvelope(targetGain, envelope_, attacking);
        lastAttackTime_ = currentTime;
        
        // 結果を設定
        result.targetGain = targetGain;
        result.currentGain = envelope_;
        result.outputGain = envelope_;
        
        currentDuckAmount_ = linearToDb(envelope_);
        
        return result;
    }

    // サンプルを処理（複数トラック）
    QMap<int, DuckingResult> AudioDucking::processAllTracks(const QMap<int, AudioTrackInfo>& trackLevels)
    {
        QMap<int, DuckingResult> results;
        
        if (!settings_.enabled || mainSourceTrackId_ < 0) {
            // 無効またはメインソースがない場合はゲイン1.0
            for (auto it = trackLevels.constBegin(); it != trackLevels.constEnd(); ++it) {
                DuckingResult result;
                result.outputGain = 1.0f;
                results[it.key()] = result;
            }
            return results;
        }

        // メインソースのレベルを取得
        float mainLevel = -INFINITY;
        if (trackLevels.contains(mainSourceTrackId_)) {
            mainLevel = trackLevels[mainSourceTrackId_].currentLevel;
        }

        // 各トラックを処理
        for (auto it = trackLevels.constBegin(); it != trackLevels.constEnd(); ++it) {
            int trackId = it.key();
            
            // メインソースはダッキングしない
            if (trackId == mainSourceTrackId_) {
                DuckingResult result;
                result.outputGain = 1.0f;
                results[trackId] = result;
                continue;
            }

            // ダッキング対象かどうか
            if (!duckingTargets_.contains(trackId)) {
                DuckingResult result;
                result.outputGain = 1.0f;
                results[trackId] = result;
                continue;
            }

            // メインソースの存在を検出
            float targetGain = calculateGain(mainLevel, it.value().currentLevel);
            
            DuckingResult result;
            result.targetGain = targetGain;
            
            // エンベロープを適用
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            bool attacking = targetGain < envelope_;
            envelope_ = applyEnvelope(targetGain, envelope_, attacking);
            lastAttackTime_ = currentTime;
            
            result.currentGain = envelope_;
            result.outputGain = envelope_;
            result.isDucking = envelope_ < 1.0f;
            result.duckingAmount = linearToDb(envelope_);
            
            results[trackId] = result;
        }
        
        // 状態を更新
        bool wasDucking = isDucking_;
        isDucking_ = envelope_ < 0.99f;
        
        if (wasDucking != isDucking_) {
            emit duckingStateChanged(isDucking_, currentDuckAmount_);
        }

        return results;
    }

    // ゲインを計算
    float AudioDucking::calculateGain(float mainLevel, float backgroundLevel)
    {
        if (!settings_.enabled) {
            return 1.0f;
        }

        // メインソースがしきい値を超えているか
        if (mainLevel > settings_.threshold) {
            // ダッキング開始
            float reduction = mainLevel - settings_.threshold;
            float targetDb = settings_.duckLevel + (reduction * (1.0f - settings_.ratio));
            return dbToLinear(targetDb);
        }
        
        // ダッキング解除
        return 1.0f;
    }

    // プレゼンスを検出
    float AudioDucking::detectPresence(const float* samples, size_t count)
    {
        if (!samples || count == 0) {
            return 0.0f;
        }

        switch (settings_.detection) {
        case PresenceDetection::RMS:
            return calculateRMS(samples, count);
            
        case PresenceDetection::Peak:
            return calculatePeak(samples, count);
            
        case PresenceDetection::Threshold:
        default:
            // RMSを使用
            return calculateRMS(samples, count);
        }
    }

    // エンベロープを適用
    float AudioDucking::applyEnvelope(float target, float current, bool attacking)
    {
        float envelopeTime = attacking ? 
            static_cast<float>(settings_.attackTime) / 1000.0f : 
            static_cast<float>(settings_.releaseTime) / 1000.0f;
        
        float step = 1.0f / (envelopeTime * 48000.0f); // 48kHz仮定
        
        if (attacking) {
            // アタック中（ターゲット更低）
            return std::max(target, current - step);
        } else {
            // リリース中（ターゲット更高）
            return std::min(target, current + step);
        }
    }

    // しきい値をチェック
    bool AudioDucking::checkThreshold(float level)
    {
        return level > settings_.threshold;
    }

    // レベルをdBに変換
    float AudioDucking::linearToDb(float linear)
    {
        if (linear <= 0.0f) {
            return -INFINITY;
        }
        return 20.0f * std::log10(linear);
    }

    // リニアに変換
    float AudioDucking::dbToLinear(float db)
    {
        if (db <= -96.0f) {
            return 0.0f;
        }
        return std::pow(10.0f, db / 20.0f);
    }

    // RMSを計算
    float AudioDucking::calculateRMS(const float* samples, size_t count)
    {
        if (!samples || count == 0) {
            return 0.0f;
        }

        float sum = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            sum += samples[i] * samples[i];
        }
        
        return std::sqrt(sum / static_cast<float>(count));
    }

    // ピークを計算
    float AudioDucking::calculatePeak(const float* samples, size_t count)
    {
        if (!samples || count == 0) {
            return 0.0f;
        }

        float peak = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            float absValue = std::abs(samples[i]);
            if (absValue > peak) {
                peak = absValue;
            }
        }
        
        return peak;
    }

    // 推奨ダッキング設定を取得
    DuckingSettings AudioDucking::getRecommendedSettings()
    {
        DuckingSettings settings;
        settings.type = DuckingType::GainReduction;
        settings.detection = PresenceDetection::RMS;
        settings.duckLevel = -20.0f;
        settings.threshold = -40.0f;
        settings.attackTime = 10;
        settings.releaseTime = 300;
        settings.detectionWindow = 50;
        settings.ratio = 0.25f;
        settings.holdTime = 100;
        settings.enabled = true;
        return settings;
    }

    // DuckingTypeから名称を取得
    QString duckingTypeToName(DuckingType type)
    {
        switch (type) {
        case DuckingType::None: return "None";
        case DuckingType::Compress: return "Compressor";
        case DuckingType::GainReduction: return "Gain Reduction";
        case DuckingType::Sidechain: return "Sidechain";
        default: return "Unknown";
        }
    }

    // PresenceDetectionから名称を取得
    QString detectionTypeToName(PresenceDetection detection)
    {
        switch (detection) {
        case PresenceDetection::Threshold: return "Threshold";
        case PresenceDetection::RMS: return "RMS";
        case PresenceDetection::Peak: return "Peak";
        default: return "Unknown";
        }
    }

} // namespace ArtifactCore
