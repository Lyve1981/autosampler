#pragma once

#include "audioData.h"
#include "config.h"

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
	explicit AutoSampler(Config _config);
	virtual ~AutoSampler();
	void run();
	bool audioInputCallback(const void* _input, size_t _frameCount);

	void writeWaveFile(autosampler::AudioData* _data, int _program, int _note, int _velocity);
	
private:
	void initAudioInput();
	void initMidiOutput();
	void sendMidi(uint8_t a, uint8_t b, uint8_t c) const;
	void setState(State _state);
	
	const Config m_config;
	void* m_inputStream = nullptr;
	void* m_outputStream = nullptr;

	float m_samplerate;

	std::unique_ptr<autosampler::AudioData> m_audioData;

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
};
