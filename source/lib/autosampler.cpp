
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
#include <direct.h>


#include "wavWriter.h"
#include "../base/logging.h"

#include "../portaudio/include/portaudio.h"

#include "../portmidi/pm_common/portmidi.h"

namespace asLib
{
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

		::_mkdir(path.c_str());		
	}
}

AutoSampler::AutoSampler(Config _config) : m_config(std::move(_config))
{
	Pa_Initialize();
	Pm_Initialize();

	initMidiOutput();

	initAudioInput();

	m_detectNoiseFloorDuration = static_cast<int>(m_config.detectNoisefloorDuration * m_samplerate);
	m_pauseBefore = static_cast<int>(m_config.pauseBefore * m_samplerate);
	m_sustainLength = static_cast<int>(m_config.sustainLength * m_samplerate);
	m_releaseLength = static_cast<int>(m_config.releaseLength * m_samplerate);
	m_pauseAfter = static_cast<int>(m_config.pauseAfter * m_samplerate);

	m_audioData->reserve((m_sustainLength + m_releaseLength) << 1);	 // a bit extra, block size causes lengths to be exceeded

	m_currentProgram = -1;

	m_currentNote = static_cast<int>(m_config.noteNumbers.size());
	m_currentVelocity = static_cast<int>(m_config.velocities.size());

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
	const auto devCount = Pa_GetDeviceCount();

	std::vector<PaDeviceIndex> matchingDevices;
	matchingDevices.reserve(devCount);
	
	for (auto i = 0; i < devCount; ++i)
	{
		const auto* devInfo = Pa_GetDeviceInfo(i);

		if(devInfo->maxInputChannels <= m_config.inputChannels)
			continue;

		if(!m_config.inputDevice.empty() && !strequal(m_config.inputDevice, devInfo->name) != 0)
			continue;

		const auto* hostApi = Pa_GetHostApiInfo(devInfo->hostApi);

		if(!m_config.inputHostApi.empty() && !strequal(m_config.inputHostApi, hostApi->name) != 0)
			continue;

		LOG("Audio Input Device " << i << " [" << hostApi->name << "]: " << devInfo->name << ", default samplerate " << devInfo->defaultSampleRate << ", max input channels " << devInfo->maxInputChannels)

		matchingDevices.push_back(i);
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
	const auto devCount = Pm_CountDevices();

	std::vector<int> matchingDevices;

	for(auto i=0; i<devCount; ++i)
	{
		const auto* devInfo = Pm_GetDeviceInfo(i);

		if(!devInfo->output)
			continue;

		if(!m_config.midiOutputDevice.empty() && !strequal(m_config.midiOutputDevice, devInfo->name) != 0)
			continue;

		if(!m_config.midiOutputApi.empty() && !strequal(m_config.midiOutputApi, devInfo->interf) != 0)
			continue;

		LOG("MIDI device " << i << ": [" << devInfo->interf << "]: " << devInfo->name);

		matchingDevices.push_back(i);
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
		LOG("Entering pause before stage");
		m_audioData->clear();
		++m_currentVelocity;
		if(m_currentVelocity >= m_config.velocities.size())
		{
			m_currentVelocity = 0;
			++m_currentNote;

			if(m_currentNote >= m_config.noteNumbers.size())
			{
				m_currentNote = 0;
				++m_currentProgram;

				if(m_currentProgram >= m_config.programChanges.size())
				{
					setState(Finished);
				}
				else
				{
					auto program = m_config.programChanges[m_currentProgram];
					LOG("Sending program change " << static_cast<int>(program));
					sendMidi(M_PROGRAMCHANGE, program, 0);
				}
			}
		}
		break;
	case Sustain:
		LOG("Entering sustain stage");
		m_audioData->clear();
		sendMidi(M_NOTEON, m_config.noteNumbers[m_currentNote], m_config.velocities[m_currentVelocity]);
		break;
	case Release:
		LOG("Entering release stage");
		sendMidi(M_NOTEOFF, m_config.noteNumbers[m_currentNote], m_config.releaseVelocity);
		break;
	case PauseAfter:
		{
			LOG("Entering pause after stage");

			auto* data = m_audioData->clone();

			PendingWrite pendingWrite;

			pendingWrite.data.reset(data);
			pendingWrite.thread.reset(new std::thread(&AutoSampler::writeWaveFile, this, pendingWrite.data.get(), m_currentProgram, m_currentNote, m_currentVelocity));

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

void AutoSampler::writeWaveFile(AudioData* _data, int _program, int _note, int _velocity)
{
	_program = m_config.programChanges[_program];
	_note = m_config.noteNumbers[_note];
	_velocity = m_config.velocities[_velocity];

	auto filename = m_config.filename;

	{
		std::stringstream ss; ss << std::setw(3) << std::setfill('0') << _program;
		strreplace(filename, "{program}", ss.str());
	}
	{
		std::stringstream ss; ss << std::setw(3) << std::setfill('0') << _note;
		strreplace(filename, "{note}", ss.str());
	}
	{
		std::stringstream ss; ss << std::setw(3) << std::setfill('0') << _velocity;
		strreplace(filename, "{velocity}", ss.str());
	}
	{
		const char* keys[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

		const auto octave = _note / 12;
		const auto n = _note - octave * 12;

		std::stringstream ss; ss << keys[n] << (octave - 2);	// C-2 to G8
		strreplace(filename, "{key}", ss.str());
	}

	
	const auto splitPos = filename.find_last_of("/\\");
	const auto file = splitPos != std::string::npos ? filename.substr(splitPos+1) : filename;

	LOG("Trimming file " << file);
	
	createDirectoryRecursive(filename);

	_data->trimStart(m_noiseFloor * 1.05f);
	_data->trimEnd(m_noiseFloor * 1.05f);

	if(!_data->empty())
	{
		LOG("Writing file " << filename);
		const auto writeRes = WavWriter::write(filename, _data->data(), _data->getBitsPerSample(), _data->getIsFloat(), static_cast<int>(_data->getChannelCount()), static_cast<int>(m_samplerate), nullptr);
		if(!writeRes)
			LOG("Failed to create file " << filename);
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
						const auto g = std::fabsf(m_audioData->floatValue(f, c));
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
				setState(PauseBefore);
			break;
		case Finished:
			return false;	// stop
	}

	return true;	// want more
}
}
