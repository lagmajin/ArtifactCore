module;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QJsonArray>
#include <QJsonObject>
module Audio.Bus;

import Utils.String.UniString;
import Core.ArtifactString;
import Utils.String.Like;
import Audio.Segment;
import Audio.DownMixer;
import Audio.Effect;
import Audio.Effect.Compressor;
import Memory.TrackedPtr;
import Container.NamedVector;


namespace ArtifactCore {

	float sanitizeBusSample(float sample)
	{
		if (std::isfinite(sample)) return sample;
		if (std::isnan(sample)) return 0.0f;
		return std::copysign(std::numeric_limits<float>::max(), sample);
	}

	float accumulateBusSample(float current, float input, float gain)
	{
		const float safeCurrent = sanitizeBusSample(current);
		const float safeInput = sanitizeBusSample(input);
		const float contribution = sanitizeBusSample(safeInput * gain);
		return sanitizeBusSample(safeCurrent + contribution);
	}

	struct MeterState {
		float peak = 0.0f;
		float rms = 0.0f;
	};

	class AudioBus::Impl {
	public:
		Id id_;
		ZeroString name_;
		AudioChannelLayout layout_ = AudioChannelLayout::Stereo;
		float volumeDb_ = 0.0f;
		float pan_ = 0.0f;
		PanningMode panningMode_ = PanningMode::EqualPower;
		bool mute_ = false;
		bool solo_ = false;
		
		NamedVector<SharedPtr<AudioEffect>> effects_{
			makeNamedVector<SharedPtr<AudioEffect>>(ContainerName{"AudioBusEffects"})};

		NamedVector<MeterState> meters{
			makeNamedVector<MeterState>(ContainerName{"AudioBusMeters"})};
		qint64 compensationDelaySamples_ = 0;
		int compensationSampleRate_ = 0;
		std::vector<std::deque<float>> compensationHistory_;

		float getLinearGain() const {
			if (volumeDb_ <= -144.0f) return 0.0f;
			return std::pow(10.0f, volumeDb_ / 20.0f);
		}

		AudioSegment mainBuffer_;
		AudioSegment sideChainBuffer_;
		mutable std::unique_ptr<AudioDownMixer> downMixer_;
		ZeroString sidechainSource_;

		AudioDownMixer& getDownMixer() const {
			if (!downMixer_) downMixer_ = std::make_unique<AudioDownMixer>();
			return *downMixer_;
		}
	};


	AudioBus::AudioBus()
		: impl_(new Impl())
	{
	}

	AudioBus::~AudioBus()
	{
		delete impl_;
	}

	void AudioBus::setName(const ZeroString& name)
	{
		impl_->name_ = name;
	}

	void AudioBus::setName(const UniString& name)
	{
		impl_->name_ = ZeroString(name.toQString().toUtf8().constData());
	}

	ZeroString AudioBus::getName() const
	{
		return impl_->name_;
	}

	UniString AudioBus::getNameUni() const
	{
		return UniString(impl_->name_.data());
	}

	void AudioBus::setLayout(AudioChannelLayout layout)
	{
		switch (layout) {
			case AudioChannelLayout::Mono:
			case AudioChannelLayout::Stereo:
			case AudioChannelLayout::Surround51:
			case AudioChannelLayout::Surround71:
			case AudioChannelLayout::Custom10ch:
				impl_->layout_ = layout;
				break;
			default:
				impl_->layout_ = AudioChannelLayout::Stereo;
				break;
		}
	}

	AudioChannelLayout AudioBus::getLayout() const
	{
		return impl_->layout_;
	}

	void AudioBus::setVolume(float db)
	{
		impl_->volumeDb_ = std::isfinite(db) ? std::clamp(db, -144.0f, 24.0f) : 0.0f;
	}

	float AudioBus::getVolume() const
	{
		return impl_->volumeDb_;
	}

	void AudioBus::setPan(float pan)
	{
		impl_->pan_ = std::isfinite(pan) ? std::clamp(pan, -1.0f, 1.0f) : 0.0f;
	}

	float AudioBus::getPan() const
	{
		return impl_->pan_;
	}

	void AudioBus::setPanningMode(PanningMode mode)
	{
		impl_->panningMode_ = mode;
	}

	PanningMode AudioBus::getPanningMode() const
	{
		return impl_->panningMode_;
	}

	void AudioBus::setMute(bool mute)
	{
		impl_->mute_ = mute;
	}

	bool AudioBus::isMute() const
	{
		return impl_->mute_;
	}

	void AudioBus::setSolo(bool solo)
	{
		impl_->solo_ = solo;
	}

	bool AudioBus::isSolo() const
	{
		return impl_->solo_;
	}

	void AudioBus::addEffect(SharedPtr<AudioEffect> effect)
	{
		if (effect) {
			impl_->effects_.append(effect);
		}
	}

	void AudioBus::removeEffect(int index)
	{
		if (index >= 0 && index < impl_->effects_.size()) {
			impl_->effects_.takeAt(static_cast<std::size_t>(index));
		}
	}

	int AudioBus::getEffectCount() const
	{
		return static_cast<int>(impl_->effects_.size());
	}

	SharedPtr<AudioEffect> AudioBus::getEffect(int index) const
	{
		if (index >= 0 && index < impl_->effects_.size()) {
			return impl_->effects_[index];
		}
		return nullptr;
	}

	qint64 AudioBus::latencySamples() const
	{
		qint64 total = 0;
		for (const auto& effect : impl_->effects_) {
			if (!effect || effect->isBypassed()) continue;
			const qint64 value = effect->latencySamples();
			if (value > 0 && total <= std::numeric_limits<qint64>::max() - value) {
				total += value;
			}
		}
		return total;
	}

	qint64 AudioBus::tailSamples() const
	{
		qint64 maximum = 0;
		for (const auto& effect : impl_->effects_) {
			if (!effect || effect->isBypassed()) continue;
			maximum = std::max(maximum, std::max<qint64>(0, effect->tailSamples()));
		}
		return maximum;
	}

	void AudioBus::applyLatencyCompensation(qint64 samples)
	{
		const int channels = impl_->mainBuffer_.channelCount();
		const int frames = impl_->mainBuffer_.frameCount();
		const qint64 delay = std::max<qint64>(0, samples);
		if (channels <= 0 || frames <= 0 || delay == 0) {
			if (delay == 0 && impl_->compensationDelaySamples_ != 0) {
				impl_->compensationHistory_.clear();
				impl_->compensationDelaySamples_ = 0;
				impl_->compensationSampleRate_ = 0;
			}
			return;
		}
		if (delay != impl_->compensationDelaySamples_ ||
		    impl_->compensationSampleRate_ != impl_->mainBuffer_.sampleRate ||
		    static_cast<int>(impl_->compensationHistory_.size()) != channels) {
			impl_->compensationHistory_.clear();
			impl_->compensationHistory_.resize(channels);
			impl_->compensationDelaySamples_ = delay;
			impl_->compensationSampleRate_ = impl_->mainBuffer_.sampleRate;
			for (auto& history : impl_->compensationHistory_) {
				if (delay <= static_cast<qint64>(std::numeric_limits<int>::max())) {
					history.resize(static_cast<size_t>(delay), 0.0f);
				}
			}
		}
		for (int channel = 0; channel < channels; ++channel) {
			auto& history = impl_->compensationHistory_[channel];
			if (history.empty()) continue;
			for (int frame = 0; frame < frames; ++frame) {
				const float current = sanitizeBusSample(
					impl_->mainBuffer_.channelData[channel][frame]);
				const float delayed = sanitizeBusSample(history.front());
				history.pop_front();
				history.push_back(current);
				impl_->mainBuffer_.channelData[channel][frame] = delayed;
			}
		}
	}

	// PERF: この関数は AudioMixer の topological sort で複数バス分呼ばれる。
	// volume/metering ループは scalar で、バス数 × サンプル数 のコスト。
	// SIMD (SSE/AVX) 化で 4-8 倍の高速化が見込める。
	// 参照: docs/AUDIO_PERFORMANCE_ARCHITECTURE_2026-06-05.md
	void AudioBus::process(AudioSegment& segment)
	{
		// 1. Apply FX Rack FIRST (Pre-fader)
		for (auto& effect : impl_->effects_) {
			if (effect && !effect->isBypassed()) {
				effect->process(segment, &impl_->sideChainBuffer_);
			}
		}

		// 2. Apply Volume and Pan (Post-fader)
		int channels = segment.channelCount();
		if (impl_->meters.size() != channels) {
			impl_->meters.resize(channels);
		}

		if (impl_->mute_) {
			for (int c = 0; c < channels; ++c) {
				segment.channelData[c].fill(0.0f);
				impl_->meters[c].peak = 0.0f;
				impl_->meters[c].rms = 0.0f;
			}
			return;
		}

		float linearGain = impl_->getLinearGain();
		
		// チャンネルゲインの算出
		std::vector<float> channelGains(channels, 1.0f);
		if (channels == 2) {
			if (impl_->panningMode_ == PanningMode::EqualPower) {
				auto pg = AudioPanner::calculateConstantPowerGains(impl_->pan_);
				channelGains = pg.channelGains;
			} else {
				// Linear Balance
				channelGains[0] = (impl_->pan_ <= 0.0f) ? 1.0f : (1.0f - impl_->pan_);
				channelGains[1] = (impl_->pan_ >= 0.0f) ? 1.0f : (1.0f + impl_->pan_);
			}
		}

		for (int c = 0; c < channels; ++c) {
			float channelGain = linearGain * (c < channelGains.size() ? channelGains[c] : 1.0f);

			float* data = segment.channelData[c].data();
			const int samples = segment.channelData[c].size();
			float peak = 0.0f;
			double sumSq = 0.0;

			// Gain apply — auto‑vectorized by MSVC at /O2 (ivdep = no alias)
			__pragma(loop(ivdep))
			for (int i = 0; i < samples; ++i) {
				if (!std::isfinite(data[i])) {
					data[i] = 0.0f;
				} else {
					const float scaled = data[i] * channelGain;
					data[i] = std::isfinite(scaled)
						? scaled
						: std::copysign(std::numeric_limits<float>::max(), scaled);
				}
			}

			// Metering (reduction makes auto‑vectorization harder; keep separate)
			for (int i = 0; i < samples; ++i) {
				float val = data[i];
				float absVal = std::abs(val);
				if (absVal > peak) peak = absVal;
				sumSq += static_cast<double>(val) * val;
			}

			impl_->meters[c].peak = peak;
			const double rms = samples > 0 ? std::sqrt(sumSq / samples) : 0.0;
			impl_->meters[c].rms = std::isfinite(rms)
				? static_cast<float>(std::min(
					rms, static_cast<double>(std::numeric_limits<float>::max())))
				: std::numeric_limits<float>::max();

			// Soft-clip after metering to catch accumulation overflow
			if (peak > 0.9f) {
				for (int i = 0; i < samples; ++i) {
					float& s = data[i];
					float absS = std::abs(s);
					if (absS > 0.9f) {
						float t = std::min(10.0f, (absS - 0.9f) / 0.1f);
						// Pade [2/2] approximation of tanh:  x*(27+x²)/(27+9x²)
						float t2 = t * t;
						float fastTanh = t * (27.0f + t2) / (27.0f + 9.0f * t2);
						s = (s >= 0.0f ? 1.0f : -1.0f) * (0.9f + 0.1f * fastTanh);
					}
				}
			}
		}
	}

	void AudioBus::clearInput(int frameCount, int sampleRate)
	{
		const int safeFrameCount = std::max(0, frameCount);
		const int safeSampleRate = sampleRate > 0 ? sampleRate : 44100;
		int chCount = 2; // Default
		switch (impl_->layout_) {
			case AudioChannelLayout::Mono: chCount = 1; break;
			case AudioChannelLayout::Stereo: chCount = 2; break;
			case AudioChannelLayout::Surround51: chCount = 6; break;
			case AudioChannelLayout::Surround71: chCount = 8; break;
			case AudioChannelLayout::Custom10ch: chCount = 10; break;
			default: chCount = 2; break;
		}

		if (impl_->mainBuffer_.channelCount() != chCount) impl_->mainBuffer_.channelData.resize(chCount);
		if (impl_->sideChainBuffer_.channelCount() != chCount) impl_->sideChainBuffer_.channelData.resize(chCount);

		impl_->mainBuffer_.sampleRate = safeSampleRate;
		impl_->mainBuffer_.layout = impl_->layout_;
		impl_->sideChainBuffer_.sampleRate = safeSampleRate;
		impl_->sideChainBuffer_.layout = impl_->layout_;

		for (int c = 0; c < chCount; ++c) {
			impl_->mainBuffer_.channelData[c].resize(safeFrameCount);
			impl_->mainBuffer_.channelData[c].fill(0.0f);
			impl_->sideChainBuffer_.channelData[c].resize(safeFrameCount);
			impl_->sideChainBuffer_.channelData[c].fill(0.0f);
		}
	}

	void AudioBus::addInput(const AudioSegment& input, float localGain)
	{
		if (!std::isfinite(localGain)) {
			return;
		}
		const AudioSegment* source = &input;
		AudioSegment downmixed;

		// Normalize both metadata and channel shape before mixing. A malformed
		// segment can report the same layout while carrying a different number
		// of channels, which must not bypass the downmixer.
		if (input.layout != impl_->layout_ ||
			input.channelCount() != impl_->mainBuffer_.channelCount()) {
			impl_->getDownMixer().setTargetLayout(impl_->layout_);
			downmixed = impl_->getDownMixer().process(input);
			source = &downmixed;
		}

		int channels = std::min((int)source->channelData.size(), (int)impl_->mainBuffer_.channelData.size());
		int frames = std::min(source->frameCount(), impl_->mainBuffer_.frameCount());
		
		for (int c = 0; c < channels; ++c) {
			const float* src = source->channelData[c].constData();
			float* dst = impl_->mainBuffer_.channelData[c].data();
			const int channelFrames = std::min(
				frames,
				std::min(static_cast<int>(source->channelData[c].size()),
						 static_cast<int>(impl_->mainBuffer_.channelData[c].size())));
			for (int i = 0; i < channelFrames; ++i) {
				dst[i] = accumulateBusSample(dst[i], src[i], localGain);
			}
		}
	}

	void AudioBus::addSideChain(const AudioSegment& input, float localGain)
	{
		if (!std::isfinite(localGain)) {
			return;
		}
		const AudioSegment* source = &input;
		AudioSegment downmixed;

		if (input.layout != impl_->layout_ ||
			input.channelCount() != impl_->sideChainBuffer_.channelCount()) {
			impl_->getDownMixer().setTargetLayout(impl_->layout_);
			downmixed = impl_->getDownMixer().process(input);
			source = &downmixed;
		}

		int channels = std::min((int)source->channelData.size(), (int)impl_->sideChainBuffer_.channelData.size());
		int frames = std::min(source->frameCount(), impl_->sideChainBuffer_.frameCount());

		for (int c = 0; c < channels; ++c) {
			const float* src = source->channelData[c].constData();
			float* dst = impl_->sideChainBuffer_.channelData[c].data();
			const int channelFrames = std::min(
				frames,
				std::min(static_cast<int>(source->channelData[c].size()),
						 static_cast<int>(impl_->sideChainBuffer_.channelData[c].size())));
			for (int i = 0; i < channelFrames; ++i) {
				dst[i] = accumulateBusSample(dst[i], src[i], localGain);
			}
		}
	}

	AudioSegment& AudioBus::getOutputBuffer()
	{
		return impl_->mainBuffer_;
	}

	const AudioSegment& AudioBus::getSideChainBuffer() const
	{
		return impl_->sideChainBuffer_;
	}

	float AudioBus::getPeakLevel(int channelIndex) const
	{
		if (channelIndex < 0 || channelIndex >= impl_->meters.size()) return 0.0f;
		return impl_->meters[channelIndex].peak;
	}

	float AudioBus::getRMSLevel(int channelIndex) const
	{
		if (channelIndex < 0 || channelIndex >= impl_->meters.size()) return 0.0f;
		return impl_->meters[channelIndex].rms;
	}

	float AudioBus::getGainReduction() const
	{
	float gainReduction = 1.0f;
	for (auto& effect : impl_->effects_) {
		if (auto comp = ArtifactCore::dynamicPointerCast<AudioCompressor>(effect)) {
			const float reduction = comp->getGainReduction();
			if (std::isfinite(reduction)) {
				gainReduction = std::min(gainReduction, std::clamp(reduction, 0.0f, 1.0f));
			}
		}
	}
	return gainReduction; // No reduction = 1.0
	}

	const Id& AudioBus::id() const
	{
		return impl_->id_;
	}

	void AudioBus::restoreId(const Id& id)
	{
		if (!id.isNil()) impl_->id_ = id;
	}
	void AudioBus::setSidechainSource(const String& busName)
	{
		impl_->sidechainSource_ = ZeroString(busName.data(), busName.length());
	}

	void AudioBus::setSidechainSource(const UniString& busName)
	{
		impl_->sidechainSource_ = ZeroString(static_cast<std::string>(busName));
	}

	String AudioBus::getSidechainSource() const
	{
		return String(impl_->sidechainSource_.data(), impl_->sidechainSource_.length());
	}

	ZeroString AudioBus::getSidechainSourceZero() const
	{
		return impl_->sidechainSource_;
	}

	QJsonObject AudioBus::toJson() const
	{
		QJsonObject obj;
		obj["id"] = impl_->id_.toQString();
		obj["name"] = toQString(impl_->name_);
		obj["volume_db"] = impl_->volumeDb_;
		obj["pan"] = impl_->pan_;
		obj["layout"] = static_cast<int>(impl_->layout_);
		obj["mute"] = impl_->mute_;
		obj["solo"] = impl_->solo_;
		obj["sidechain_source"] = toQString(impl_->sidechainSource_);

		QJsonArray effects;
		for (size_t i = 0; i < impl_->effects_.size(); ++i) {
			auto& eff = impl_->effects_[i];
			QJsonObject effectObj = eff->toJson();
			effectObj["slot"] = static_cast<int>(i);
			effects.append(effectObj);
		}
		obj["effects"] = effects;

		return obj;
	}

	void AudioBus::fromJson(const QJsonObject& obj)
	{
		if (obj.contains("id") && !obj["id"].toString().trimmed().isEmpty()) {
			impl_->id_ = Id(obj["id"].toString());
		}
		if (obj.contains("name")) {
			setName(ZeroString(obj["name"].toString().toUtf8().constData()));
		}
		setVolume(static_cast<float>(obj["volume_db"].toDouble(0.0)));
		setPan(static_cast<float>(obj["pan"].toDouble(0.0)));
		if (obj.contains("layout")) {
			setLayout(static_cast<AudioChannelLayout>(obj["layout"].toInt(
				static_cast<int>(AudioChannelLayout::Stereo))));
		}
		impl_->mute_ = obj["mute"].toBool(false);
		impl_->solo_ = obj["solo"].toBool(false);
		impl_->sidechainSource_ = ZeroString(obj["sidechain_source"].toString().toUtf8().constData());
		// Note: effects are loaded externally using the manager+factory pattern
		// because the bus doesn't own a manager reference
	}

};
