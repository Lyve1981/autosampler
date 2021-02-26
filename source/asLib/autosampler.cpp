
#include "autosampler.h"
#include "midiTypes.h"
#include "audioData.h"

#include <algorithm>
#include <cassert>
#include <iomanip>


#include "config.h"
#include "error.h"

#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#define _mkdir(PATH) mkdir(PATH, 660)
#endif

#include <iostream>

#include "wavWriter.h"
#include "../asBase/logging.h"

#include "../portaudio/include/portaudio.h"

#include "../portmidi/pm_common/portmidi.h"

namespace asLib
{
constexpr float g_noiseFloorFactor = 1.25f;
constexpr uint8_t g_programChangeNone = 0xff;
	
static int portAudioCallback(const void* _inputBuffer, void*, const unsigned long _framesPerBuffer, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* _userData)
{
	auto* sampler = static_cast<AutoSampler*>(_userData);
	if(sampler->audioInputCallback(_inputBuffer, _framesPerBuffer))
		return paContinue;
	return paComplete;
}

static int bitCountToSampleFormat(int bitCount)
{
	switch (bitCount)
	{
	case 8:		return paInt8;
	case 16:	return paInt16;
	case 24:	return paInt24;
	case 32:	return paFloat32;
	default:
		throw Error(ErrInvalidInputBitCount, "Invalid bit count for input specified (should be 8, 16, 24 or 32");
	}
}

bool strequal(const std::string& _a, const std::string& _b)
{
    return _a.size() == _b.size() && std::equal(_a.cbegin(), _a.cend(), _b.cbegin(),[](const std::string::value_type& _a, const std::string::value_type& _b)
    {
		return toupper(_a) == toupper(_b);
    });
}

void strreplace(std::string& _string, const std::string& _search, const std::string& _replacement)
{
	for(size_t searchPos = 0; searchPos < _string.size(); )
	{
		auto pos = _string.find(_search, searchPos);

		if(pos == std::string::npos)
			break;

		_string.replace(pos, _search.size(), _replacement);

		searchPos = pos + _replacement.size();
	}
}

void createDirectoryRecursive(const std::string& filename)
{
	for(size_t searchPos=0; searchPos < filename.size();)
	{
		size_t pos = filename.find_first_of("/\\", searchPos);

		if(pos == std::string::npos)
			break;

		const auto path = filename.substr(0,pos);

		searchPos = pos + 1;

		if(path.back() == ':')
			continue;	// skip windows drive letter

		_mkdir(path.c_str());		
	}
}

std::string noteToString(uint8_t _note)
{
	const char* keys[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

	const auto octave = _note / 12;
	const auto n = _note - octave * 12;

	std::stringstream ss; ss << keys[n] << (octave - 2);	// C-2 to G8
	return ss.str();
}
	
static bool s_apisInitialized;

static void initApis()
{
	if(s_apisInitialized)
		return;
	
	Pa_Initialize();
	Pm_Initialize();

	s_apisInitialized = true;	
}

AutoSampler::AutoSampler(Config _config) : m_config(std::move(_config))
{
	initApis();

	generateVoices();

	initMidiOutput();

	initAudioInput();

	m_detectNoiseFloorDuration = static_cast<int>(m_config.detectNoisefloorDuration * m_samplerate);
	m_pauseBefore = static_cast<int>(m_config.pauseBefore * m_samplerate);
	m_sustainLength = static_cast<int>(m_config.sustainLength * m_samplerate);
	m_releaseLength = static_cast<int>(m_config.releaseLength * m_samplerate);
	m_pauseAfter = static_cast<int>(m_config.pauseAfter * m_samplerate);

	m_audioData->reserve((m_sustainLength + m_releaseLength) << 1);	 // a bit extra, block size causes lengths to be exceeded

	setState(DetectNoiseFloor);

	Pa_StartStream(m_inputStream);
}

AutoSampler::~AutoSampler()
{
	if(m_inputStream)
	{
		Pa_CloseStream(m_inputStream);
		m_inputStream = nullptr;		
	}

	if(m_outputStream)
	{
		Pm_Close(m_outputStream);
		m_outputStream = nullptr;
	}

	Pm_Terminate();
	Pa_Terminate();

	s_apisInitialized = false;
}

void AutoSampler::run()
{
	while(true)
	{
		{
			std::lock_guard<std::mutex> lockPendingWrites(m_lockPendingWrites);
			for(auto it = m_pendingWrites.begin(); it != m_pendingWrites.end();)
			{
				const auto& pendingWrite = it->second;

				if(pendingWrite.data == nullptr)
				{
					pendingWrite.thread->join();
					m_pendingWrites.erase(it++);
				}
				else
					++it;
			}

			if(m_state == Finished && m_pendingWrites.empty())
				break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

void AutoSampler::initAudioInput()
{
	std::vector<AudioDeviceInfo> audioDevices;
	getAudioInputs(audioDevices);

	std::vector<PaDeviceIndex> matchingDevices;
	matchingDevices.reserve(audioDevices.size());
	
	for (auto i = 0; i < audioDevices.size(); ++i)
	{
		const auto& devInfo = audioDevices[i];

		if(devInfo.maxChannels <= m_config.inputChannels)
			continue;

		if(!m_config.inputDevice.empty() && !strequal(m_config.inputDevice, devInfo.name) != 0)
			continue;

		if(!m_config.inputHostApi.empty() && !strequal(m_config.inputHostApi, devInfo.api) != 0)
			continue;

		LOG("Audio Input Device " << i << " [" << devInfo.api << "]: " << devInfo.name << ", default samplerate " << devInfo.maxSamplerate << ", max input channels " << devInfo.maxChannels)

		matchingDevices.push_back(devInfo.id);
	}

	if(matchingDevices.empty())
	{
		throw Error(ErrInputDeviceNotFound, "No input device found");
	}

	const auto& device = matchingDevices.back();

	PaStreamParameters inputParameters{};
	inputParameters.channelCount = m_config.inputChannels;
	inputParameters.device = device;
	inputParameters.sampleFormat = bitCountToSampleFormat(m_config.inputBits);

	auto err=  Pa_IsFormatSupported(&inputParameters, nullptr, m_config.inputSamplerate);

	if(err != paNoError)
		throw Error(ErrAudioInput, std::string("Audio Input subsystem returned error: ") + Pa_GetErrorText(err));

	const auto streamFlags = (inputParameters.sampleFormat == paFloat32) ? paClipOff : paNoFlag;

	err = Pa_OpenStream(&m_inputStream, &inputParameters, nullptr, m_config.inputSamplerate, m_config.inputBlockSize, streamFlags, portAudioCallback, this);

	if(err != paNoError)
		throw Error(ErrAudioInput, std::string("Audio Input subsystem returned error: ") + Pa_GetErrorText(err));

	const auto* streamInfo = Pa_GetStreamInfo(m_inputStream);

	// I've seen that the SR is something different than we wanted, check it
	if(static_cast<int>(streamInfo->sampleRate) != m_config.inputSamplerate)
	{
		std::stringstream ss; ss << "Failed to set samplerate, requested was " << m_config.inputSamplerate << " but we've got " << streamInfo->sampleRate;
		throw Error(ErrAudioInput, ss);
	}
	
	m_samplerate = static_cast<float>(streamInfo->sampleRate);

	m_audioData.reset(new AudioData(inputParameters.sampleFormat, inputParameters.channelCount));
}

void AutoSampler::initMidiOutput()
{
	std::vector<DeviceInfo> midiDevices;
	getMidiOutputs(midiDevices);

	std::vector<int> matchingDevices;

	for(auto i=0; i<midiDevices.size(); ++i)
	{
		const auto& devInfo = midiDevices[i];

		if(!m_config.midiOutputDevice.empty() && !strequal(m_config.midiOutputDevice, devInfo.name) != 0)
			continue;

		if(!m_config.midiOutputApi.empty() && !strequal(m_config.midiOutputApi, devInfo.api) != 0)
			continue;

		LOG("MIDI device " << i << ": [" << devInfo.api << "]: " << devInfo.name);

		matchingDevices.push_back(devInfo.id);
	}

	if(matchingDevices.empty())
		throw Error(ErrMidiOutputNotFound, "No matching midi output device found");

	const auto& device = matchingDevices.back();

	const auto err = Pm_OpenOutput(&m_outputStream, device, nullptr, 64, nullptr, nullptr, 0);

	if(err != pmNoError)
		throw Error(ErrMidiOutput, std::string("Midi Output subsystem returned error: ") + Pm_GetErrorText(err));
}

void AutoSampler::sendMidi(uint8_t a, uint8_t b, uint8_t c) const
{
	if(!m_outputStream)
		return;

	a &= 0xf0;
	a |= m_config.midiChannel & 0x0f;
	
	const auto err = Pm_WriteShort(m_outputStream, 0, Pm_Message(a,b,c));

	if(err != pmNoError)
		throw Error(ErrMidiOutput, std::string("Midi Output subsystem returned error: ") + Pm_GetErrorText(err));
}

void AutoSampler::setState(State _state)
{
	if(_state == m_state)
		return;

	m_state = _state;
	m_stateDurationInFrames = 0;

	switch (_state)
	{
	case DetectNoiseFloor:
		LOG("Detecting noise floor...");
		m_audioData->clear();
		break;
	case PauseBefore:
		{
			m_audioData->clear();

			auto program = m_voices[m_currentVoice].program;

			if(program != g_programChangeNone)
			{
				if(m_currentVoice == 0 || program != m_voices[m_currentVoice-1].program)
				{
					LOG("Sending program change " << static_cast<int>(program));
					sendMidi(M_PROGRAMCHANGE, program, 0);
				}
			}
		}
		break;
	case Sustain:
		{
			const auto& voice = m_voices[m_currentVoice];

			m_audioData->clear();
			auto note = voice.note;
			auto velocity = voice.velocity;
			LOG("Sending Note ON for note " << noteToString(note) << " (" << static_cast<int>(note) << "), velocity " << static_cast<int>(velocity));
			sendMidi(M_NOTEON, note, velocity);
		}
		break;
	case Release:
		{
			const auto& voice = m_voices[m_currentVoice];
			auto note = voice.note;

			LOG("Sending Note off for note " << noteToString(note) << " (" << static_cast<int>(note) << "), release velocity " << static_cast<int>(m_config.releaseVelocity));
			sendMidi(M_NOTEOFF, note, m_config.releaseVelocity);
		}
		break;
	case PauseAfter:
		{
			auto* data = m_audioData->clone();

			PendingWrite pendingWrite;

			pendingWrite.data.reset(data);
			pendingWrite.thread.reset(new std::thread(&AutoSampler::writeWaveFile, this, pendingWrite.data.get(), m_voices[m_currentVoice]));

			{
				std::lock_guard<std::mutex> lockPendingWrites(m_lockPendingWrites);
				m_pendingWrites.insert(std::make_pair(data, pendingWrite));
			}

			m_audioData->clear();
		}
		break;
	case Finished: 
		break;
	default:;
	}
}

void AutoSampler::writeWaveFile(AudioData* _data, const Voice& voice)
{
	const auto filename = createFilename(voice);
	
	createDirectoryRecursive(filename);

	_data->trimStart(m_noiseFloor * g_noiseFloorFactor);
	_data->trimEnd(m_noiseFloor * g_noiseFloorFactor);

	if(!_data->empty())
	{
		LOG("Writing file " << filename);
		const auto writeRes = WavWriter::write(filename, _data->data(), _data->getBitsPerSample(), _data->getIsFloat(), static_cast<int>(_data->getChannelCount()), static_cast<int>(m_samplerate), nullptr);
		if(!writeRes)
		{
			LOG("Failed to create file " << filename);
			throw Error(ErrFileIO, "Failed to create file " + filename);
		}
	}
	else
	{
		LOG("Skipping file " << filename << " as it is completely silent");
	}

	std::lock_guard<std::mutex> lockPendingWrites(m_lockPendingWrites);
	auto it = m_pendingWrites.find(_data);
	assert(it != m_pendingWrites.end());
	it->second.data.reset();	
}

std::string AutoSampler::createFilename(const Voice& voice) const
{
	auto program = m_config.programChanges.empty() ? 0 : voice.program;
	auto note = voice.note;
	auto velocity = voice.velocity;

	auto filename = m_config.filename;

	{
		std::stringstream ss; ss << std::setw(3) << std::setfill('0') << static_cast<int>(program);
		strreplace(filename, "{program}", ss.str());
	}
	{
		std::stringstream ss; ss << std::setw(3) << std::setfill('0') << static_cast<int>(note);
		strreplace(filename, "{note}", ss.str());
	}
	{
		std::stringstream ss; ss << std::setw(3) << std::setfill('0') << static_cast<int>(velocity);
		strreplace(filename, "{velocity}", ss.str());
	}

	strreplace(filename, "{key}", noteToString(note));

	return filename;
}

bool AutoSampler::getAudioInputs(std::vector<AudioDeviceInfo>& _audioInputs)
{
	initApis();
	
	const auto devCount = Pa_GetDeviceCount();

	if(devCount < 0)
		return false;

	_audioInputs.reserve(devCount);

	for (auto i = 0; i < devCount; ++i)
	{
		const auto* devInfo = Pa_GetDeviceInfo(i);
		const auto* hostApi = Pa_GetHostApiInfo(devInfo->hostApi);

		if(devInfo->maxInputChannels <= 0)
			continue;

		AudioDeviceInfo di;
		
		di.name = devInfo->name;
		di.api = hostApi->name;
		di.id = i;

		di.maxChannels = devInfo->maxInputChannels;
		di.maxSamplerate = static_cast<int>(devInfo->defaultSampleRate);

		_audioInputs.emplace_back(std::move(di));
	}

	return true;
}

bool AutoSampler::getMidiOutputs(std::vector<DeviceInfo>& _midiOutputs)
{
	initApis();

	const auto devCount = Pm_CountDevices();

	if(devCount < 0)
		return false;

	std::vector<int> matchingDevices;

	for(auto i=0; i<devCount; ++i)
	{
		const auto* devInfo = Pm_GetDeviceInfo(i);

		if(!devInfo->output)
			continue;

		DeviceInfo di;
		di.name = devInfo->name;
		di.api = devInfo->interf;
		di.id = i;

		_midiOutputs.emplace_back(std::move(di));
	}
	return true;
}

bool AutoSampler::audioInputCallback(const void* _input, size_t _frameCount)
{
	m_stateDurationInFrames += _frameCount;
	
	switch (m_state)
	{
		case DetectNoiseFloor:
			m_audioData->append(_input, _frameCount);

			if(m_stateDurationInFrames >= m_detectNoiseFloorDuration)
			{
				float gain = 0.0f;

				for(size_t f=0; f<m_audioData->lengthInFrames(); ++f)
				{
					for(size_t c=0; c<m_audioData->getChannelCount(); ++c)
					{
						const auto g = std::abs(m_audioData->floatValue(f, c));
						gain = std::max(g, gain);						
					}
				}

				LOG("Noise floor is " << gain);
				m_noiseFloor = gain;
				setState(PauseBefore);
			}
			break;
		case PauseBefore:
			if(m_stateDurationInFrames >= m_pauseBefore)
				setState(Sustain);
			break;
		case Sustain:
			m_audioData->append(_input, _frameCount);
			if(m_stateDurationInFrames >= m_sustainLength)
				setState(Release);
			break;
		case Release:
			m_audioData->append(_input, _frameCount);
			if(m_stateDurationInFrames >= m_releaseLength)
				setState(PauseAfter);
			break;
		case PauseAfter:
			if(m_stateDurationInFrames >= m_pauseAfter)
			{
				++m_currentVoice;
				if(m_currentVoice < m_voices.size())
					setState(PauseBefore);
				else
					setState(Finished);
			}
			break;
		case Finished:
			return false;	// stop
	}

	return true;	// want more
}

void AutoSampler::generateVoices()
{
	Voice voice;

	auto permutateNoteAndVelocity = [&]()
	{
		for(auto v : m_config.velocities)
		{
			voice.velocity = v;

			for(auto n : m_config.noteNumbers)
			{
				voice.note = n;

				if(m_config.skipExistingFiles)
				{
					const auto filename = createFilename(voice);

					FILE* hFile = fopen(filename.c_str(), "rb");
					if(hFile)
					{
						fclose(hFile);
						LOG("Skipping file " << filename << ", already exists")
						continue;
					}
				}

				m_voices.push_back(voice);
			}
		}		
	};

	if(m_config.programChanges.empty())
	{
		voice.program = g_programChangeNone;
		permutateNoteAndVelocity();
	}
	else
	{	
		for(auto p : m_config.programChanges)
		{
			voice.program = p;
			permutateNoteAndVelocity();
		}
	}

	std::cout << m_voices.size() << " total voices remaining" << std::endl;
}
}
