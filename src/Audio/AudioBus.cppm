module;
#include <QVector>

module Audio.Bus;
import Utils.String.UniString;
import Audio.Segment;
import std;

namespace ArtifactCore {

	struct MeterState {
		float peak = 0.0f;
		float rms = 0.0f;
	};

	class AudioBus::Impl {
	public:
		UniString name_;
		float volumeDb_ = 0.0f;
		float pan_ = 0.0f;
		bool mute_ = false;
		bool solo_ = false;

		std::vector<MeterState> meters_; // std::vector from import std;

		float getLinearGain() const {
			// -144dB as floor
			if (volumeDb_ <= -144.0f) return 0.0f;
			return std::pow(10.0f, volumeDb_ / 20.0f);
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

	void AudioBus::setName(const UniString& name)
	{
		impl_->name_ = name;
	}

	UniString AudioBus::getName() const
	{
		return impl_->name_;
	}

	void AudioBus::setVolume(float db)
	{
		impl_->volumeDb_ = db;
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

	void AudioBus::process(AudioSegment& segment)
	{
		// Update meter size if channels changed
		int channels = segment.channelData.size();
		if (impl_->meters_.size() != channels) {
			impl_->meters_.resize(channels);
		}

		if (impl_->mute_) {
			for (int c = 0; c < channels; ++c) {
				segment.channelData[c].fill(0.0f);
				impl_->meters_[c] = { 0.0f, 0.0f };
			}
			return;
		}

		float linearGain = impl_->getLinearGain();
		
		// Calculate Pan Gains (Constant Power)
		float leftPanGain = 1.0f;
		float rightPanGain = 1.0f;
		bool applyPan = (channels == 2);

		if (applyPan) {
			double pi = 3.14159265358979323846;
			double angle = (impl_->pan_ + 1.0) * pi / 4.0;
			leftPanGain = static_cast<float>(std::cos(angle));
			rightPanGain = static_cast<float>(std::sin(angle));
		}

		int samples = (channels > 0) ? segment.channelData[0].size() : 0;

		for (int c = 0; c < channels; ++c) {
			float channelGain = linearGain;
			if (applyPan) {
				if (c == 0) channelGain *= leftPanGain;
				if (c == 1) channelGain *= rightPanGain;
			}

			float* data = segment.channelData[c].data();
			float peak = 0.0f;
			float sumSq = 0.0f;

			// Basic processing loop with metering
			// TODO: SIMD optimization
			for (int i = 0; i < samples; ++i) {
				data[i] *= channelGain;
				
				float val = data[i];
				float absVal = std::abs(val);
				if (absVal > peak) peak = absVal;
				sumSq += val * val;
			}

			impl_->meters_[c].peak = peak;
			impl_->meters_[c].rms = (samples > 0) ? std::sqrt(sumSq / samples) : 0.0f;
		}
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

};