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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Audio.Bus;




import Utils.Id;
import Utils.String.Like;
import Utils.String.UniString;
import Audio.Segment;
import Audio.Panner;
import Audio.Effect;

export namespace ArtifactCore {


	class AudioBus {
	private:
		class Impl;
		Impl* impl_;
	public:
		AudioBus();
		virtual ~AudioBus();

		void setName(const UniString& name);
		UniString getName() const;

		void setLayout(AudioChannelLayout layout);
		AudioChannelLayout getLayout() const;

		// Volume in Decibels
		void setVolume(float db);
		float getVolume() const;

		// Pan: -1.0 (Left) to 1.0 (Right)
		void setPan(float pan);
		float getPan() const;

		void setPanningMode(PanningMode mode);
		PanningMode getPanningMode() const;

		void setMute(bool mute);
		bool isMute() const;

		void setSolo(bool solo);
		bool isSolo() const;

		// Effect Rack (FX Slot)
		void addEffect(std::shared_ptr<AudioEffect> effect);
		void removeEffect(int index);
		int getEffectCount() const;
		std::shared_ptr<AudioEffect> getEffect(int index) const;

		// Process audio buffer in-place
		void process(AudioSegment& segment);

		// Routing integration
		void clearInput(int frameCount, int sampleRate);
		void addInput(const AudioSegment& input, float localGain = 1.0f);
		void addSideChain(const AudioSegment& input, float localGain = 1.0f);
		
		AudioSegment& getOutputBuffer();
		const AudioSegment& getSideChainBuffer() const;

		// Metering
		float getPeakLevel(int channelIndex) const;
		float getRMSLevel(int channelIndex) const;
		float getGainReduction() const;

	};



}