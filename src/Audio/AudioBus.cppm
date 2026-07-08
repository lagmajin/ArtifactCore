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


namespace ArtifactCore {

	struct MeterState {
		float peak = 0.0f;
		float rms = 0.0f;
	};

	class AudioBus::Impl {
	public:
		ZeroString name_;
		AudioChannelLayout layout_ = AudioChannelLayout::Stereo;
		float volumeDb_ = 0.0f;
		float pan_ = 0.0f;
		PanningMode panningMode_ = PanningMode::EqualPower;
		bool mute_ = false;
		bool solo_ = false;
		
		std::vector<std::shared_ptr<AudioEffect>> effects_;

		std::vector<MeterState> meters_;

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
		impl_->layout_ = layout;
	}

	AudioChannelLayout AudioBus::getLayout() const
	{
		return impl_->layout_;
	}

	void AudioBus::setVolume(float db)
	{
		impl_->volumeDb_ = std::clamp(db, -144.0f, 24.0f);
	}

	float AudioBus::getVolume() const
	{
		return impl_->volumeDb_;
	}

	void AudioBus::setPan(float pan)
	{
		impl_->pan_ = std::clamp(pan, -1.0f, 1.0f);
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

	void AudioBus::addEffect(std::shared_ptr<AudioEffect> effect)
	{
		impl_->effects_.push_back(effect);
	}

	void AudioBus::removeEffect(int index)
	{
		if (index >= 0 && index < impl_->effects_.size()) {
			impl_->effects_.erase(impl_->effects_.begin() + index);
		}
	}

	int AudioBus::getEffectCount() const
	{
		return static_cast<int>(impl_->effects_.size());
	}

	std::shared_ptr<AudioEffect> AudioBus::getEffect(int index) const
	{
		if (index >= 0 && index < impl_->effects_.size()) {
			return impl_->effects_[index];
		}
		return nullptr;
	}

	// PERF: この関数は AudioMixer の topological sort で複数バス分呼ばれる。
	// volume/metering ループは scalar で、バス数 × サンプル数 のコスト。
	// SIMD (SSE/AVX) 化で 4-8 倍の高速化が見込める。
	// 参照: docs/AUDIO_PERFORMANCE_ARCHITECTURE_2026-06-05.md
	void AudioBus::process(AudioSegment& segment)
	{
		// 1. Apply FX Rack FIRST (Pre-fader)
		for (auto& effect : impl_->effects_) {
			if (!effect->isBypassed()) {
				effect->process(segment, &impl_->sideChainBuffer_);
			}
		}

		// 2. Apply Volume and Pan (Post-fader)
		int channels = segment.channelCount();
		if (impl_->meters_.size() != channels) {
			impl_->meters_.resize(channels);
		}

		if (impl_->mute_) {
			for (int c = 0; c < channels; ++c) {
				segment.channelData[c].fill(0.0f);
				impl_->meters_[c].peak = 0.0f;
				impl_->meters_[c].rms = 0.0f;
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

		int samples = (channels > 0) ? segment.channelData[0].size() : 0;

		for (int c = 0; c < channels; ++c) {
			float channelGain = linearGain * (c < channelGains.size() ? channelGains[c] : 1.0f);

			float* data = segment.channelData[c].data();
			float peak = 0.0f;
			float sumSq = 0.0f;

			// Gain apply — auto‑vectorized by MSVC at /O2 (ivdep = no alias)
			__pragma(loop(ivdep))
			for (int i = 0; i < samples; ++i) {
				data[i] *= channelGain;
			}

			// Metering (reduction makes auto‑vectorization harder; keep separate)
			for (int i = 0; i < samples; ++i) {
				float val = data[i];
				float absVal = std::abs(val);
				if (absVal > peak) peak = absVal;
				sumSq += val * val;
			}

			impl_->meters_[c].peak = peak;
			impl_->meters_[c].rms = samples > 0 ? std::sqrt(sumSq / samples) : 0.0f;

			// Soft-clip after metering to catch accumulation overflow
			if (peak > 0.9f) {
				for (int i = 0; i < samples; ++i) {
					float& s = data[i];
					float absS = std::abs(s);
					if (absS > 0.9f) {
						float t = (absS - 0.9f) / 0.1f;
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

		impl_->mainBuffer_.sampleRate = sampleRate;
		impl_->mainBuffer_.layout = impl_->layout_;
		impl_->sideChainBuffer_.sampleRate = sampleRate;
		impl_->sideChainBuffer_.layout = impl_->layout_;

		for (int c = 0; c < chCount; ++c) {
			impl_->mainBuffer_.channelData[c].resize(frameCount);
			impl_->mainBuffer_.channelData[c].fill(0.0f);
			impl_->sideChainBuffer_.channelData[c].resize(frameCount);
			impl_->sideChainBuffer_.channelData[c].fill(0.0f);
		}
	}

	void AudioBus::addInput(const AudioSegment& input, float localGain)
	{
		const AudioSegment* source = &input;
		AudioSegment downmixed;

		// Perform downmixing if layout differs from target bus
		if (input.layout != impl_->layout_) {
			impl_->getDownMixer().setTargetLayout(impl_->layout_);
			downmixed = impl_->getDownMixer().process(input);
			source = &downmixed;
		}

		int channels = std::min((int)source->channelData.size(), (int)impl_->mainBuffer_.channelData.size());
		int frames = std::min(source->frameCount(), impl_->mainBuffer_.frameCount());
		
		for (int c = 0; c < channels; ++c) {
			const float* src = source->channelData[c].constData();
			float* dst = impl_->mainBuffer_.channelData[c].data();
			for (int i = 0; i < frames; ++i) {
				dst[i] += src[i] * localGain;
			}
		}
	}

	void AudioBus::addSideChain(const AudioSegment& input, float localGain)
	{
		const AudioSegment* source = &input;
		AudioSegment downmixed;

		if (input.layout != impl_->layout_) {
			impl_->getDownMixer().setTargetLayout(impl_->layout_);
			downmixed = impl_->getDownMixer().process(input);
			source = &downmixed;
		}

		int channels = std::min((int)source->channelData.size(), (int)impl_->sideChainBuffer_.channelData.size());
		int frames = std::min(source->frameCount(), impl_->sideChainBuffer_.frameCount());

		for (int c = 0; c < channels; ++c) {
			const float* src = source->channelData[c].constData();
			float* dst = impl_->sideChainBuffer_.channelData[c].data();
			for (int i = 0; i < frames; ++i) {
				dst[i] += src[i] * localGain;
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
		if (channelIndex < 0 || channelIndex >= impl_->meters_.size()) return 0.0f;
		return impl_->meters_[channelIndex].peak;
	}

	float AudioBus::getRMSLevel(int channelIndex) const
	{
		if (channelIndex < 0 || channelIndex >= impl_->meters_.size()) return 0.0f;
		return impl_->meters_[channelIndex].rms;
	}

	float AudioBus::getGainReduction() const
	{
		for (auto& effect : impl_->effects_) {
			if (auto comp = std::dynamic_pointer_cast<AudioCompressor>(effect)) {
				return comp->getGainReduction();
			}
		}
		return 1.0f; // No reduction
	}

	void AudioBus::setSidechainSource(const std::string& busName)
	{
		impl_->sidechainSource_ = busName;
	}

	void AudioBus::setSidechainSource(const ZeroString& busName)
	{
		impl_->sidechainSource_ = busName;
	}

	void AudioBus::setSidechainSource(const UniString& busName)
	{
		impl_->sidechainSource_ = ZeroString(static_cast<std::string>(busName));
	}

	std::string AudioBus::getSidechainSource() const
	{
		return std::string(impl_->sidechainSource_.data(), impl_->sidechainSource_.length());
	}

	ZeroString AudioBus::getSidechainSourceZero() const
	{
		return impl_->sidechainSource_;
	}

	QJsonObject AudioBus::toJson() const
	{
		QJsonObject obj;
		obj["name"] = toQString(impl_->name_);
		obj["volume_db"] = impl_->volumeDb_;
		obj["pan"] = impl_->pan_;
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
		impl_->volumeDb_ = obj["volume_db"].toDouble(0.0);
		impl_->pan_ = obj["pan"].toDouble(0.0);
		impl_->mute_ = obj["mute"].toBool(false);
		impl_->solo_ = obj["solo"].toBool(false);
		impl_->sidechainSource_ = ZeroString(obj["sidechain_source"].toString().toUtf8().constData());
		// Note: effects are loaded externally using the manager+factory pattern
		// because the bus doesn't own a manager reference
	}

};
