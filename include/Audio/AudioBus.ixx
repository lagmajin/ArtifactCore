module;
export module Audio.Bus;

import std;
import Utils.Id;
import Utils.String.Like;
import Utils.String.UniString;
import Audio.Segment;

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

		// Volume in Decibels
		void setVolume(float db);
		float getVolume() const;

		// Pan: -1.0 (Left) to 1.0 (Right)
		void setPan(float pan);
		float getPan() const;

		void setMute(bool mute);
		bool isMute() const;

		void setSolo(bool solo);
		bool isSolo() const;

		// Process audio buffer in-place
		void process(AudioSegment& segment);

		// Metering
		float getPeakLevel(int channelIndex) const;
		float getRMSLevel(int channelIndex) const;

	};



}