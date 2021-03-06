#pragma once

#include <map>
#include <mutex>
#include <thread>

#include "audioData.h"
#include "config.h"

namespace asLib
{
class AutoSampler
{
	enum State
	{
		Invalid,
		DetectNoiseFloor,
		PauseBefore,
		Sustain,
		Release,
		PauseAfter,
		Finished,
	};
public:
	struct DeviceInfo
	{
		std::string name;
		std::string api;
		int id;
	};

	struct AudioDeviceInfo : DeviceInfo
	{
		int maxChannels;
		int maxSamplerate;
	};

	explicit AutoSampler(Config _config);
	virtual ~AutoSampler();
	void run();
	bool audioInputCallback(const void* _input, size_t _frameCount);

	void writeWaveFile(AudioData* _data, int _programIndex, int _noteIndex, int _velocityIndex);

	std::string createFilename(int _noteIndex, int _velocityIndex, int _programIndex) const;
	std::string createFilename() const
	{
		return createFilename(m_currentNote, m_currentVelocity, m_currentProgram);
	}

	static bool getAudioInputs(std::vector<AudioDeviceInfo>& _audioInputs);
	static bool getMidiOutputs(std::vector<DeviceInfo>& _midiOutputs);
	
private:
	void initAudioInput();
	void initMidiOutput();
	void sendMidi(uint8_t a, uint8_t b, uint8_t c) const;
	void setState(State _state);
	
	const Config m_config;
	void* m_inputStream = nullptr;
	void* m_outputStream = nullptr;

	float m_samplerate;

	std::unique_ptr<AudioData> m_audioData;

	State m_state = Invalid;

	size_t m_stateDurationInFrames = 0;
	size_t m_detectNoiseFloorDuration = 0;

	size_t m_pauseBefore = 0;
	size_t m_sustainLength = 0;
	size_t m_releaseLength = 0;
	size_t m_pauseAfter = 0;

	int m_currentNote = -1;
	int m_currentVelocity = -1;
	int m_currentProgram = -1;

	float m_noiseFloor = 0.0f;

	struct PendingWrite
	{
		std::shared_ptr<std::thread> thread;
		std::shared_ptr<AudioData> data;
	};

	std::mutex m_lockPendingWrites;
	std::map<const AudioData*,PendingWrite> m_pendingWrites;
};
}
