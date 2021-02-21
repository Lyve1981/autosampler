#include "cli.h"


#include <algorithm>
#include <iostream>
#include <sstream>

#include "../asLib/autosampler.h"
#include "../asLib/error.h"

namespace asCli
{
Cli::Cli(int argc, char* argv[]) : m_commandLine(argc, argv)
{
}

int Cli::run()
{
	// useful default values
	for(uint8_t i=0; i<=127; ++i)
		m_config.noteNumbers.push_back(i);

	try
	{
		registerArgument("ai-device", m_config.inputDevice, "Specify the audio device to be used to capture data. Can be empty in which case the default device is used", true, {"Windows DirectSound"});
		registerArgument("ai-api", m_config.inputHostApi, "Specify the audio host API to be used. Can be empty in which case the default api is used.", true, {"Input (High Definition Audio Device)"});
		registerArgument("ai-bitrate", m_config.inputBits, "Specify the bit depth at which audio is recorded.", true, {"16", "24", "32"});
		registerArgument("ai-samplerate", m_config.inputSamplerate, "Specify the sample rate at which audio is recorded.", true, {"44100","48000","96000"});
		registerArgument("ai-channels", m_config.inputChannels, "Specify the number of input channels that are recorded. Default mono = 1, stereo would be 2", true, {"1","2","6", "8"});
		registerArgument("ai-blocksize", m_config.inputBlockSize, "Specify the block size at which audio is processed.", true, {"512","1024","2048"});

		registerArgument("mo-device", m_config.midiOutputDevice, "Specify the MIDI device to be used to send midi data. Can be empty in which case the default device is used", true, {"MIDIOUT2 (BCR2000)"});
		registerArgument("mo-api", m_config.midiOutputApi, "Specify the MIDI host API to be used. Can be empty in which case the default api is used. On some systems, for example on Windows, there is only one API anyway.", true, {"MMSystem"});

		registerArgument("midi-notes", m_config.noteNumbers, "Specify the MIDI notes to be played. Can be specified as a single note", true, {"60", "0-127", "30,60,90"}, "0-127");
		registerArgument("midi-velocities", m_config.velocities, "Specify the velocities for note on events.", true, {"60", "0-127", "30,60,90"});
		registerArgument("midi-programs", m_config.programChanges, "A list of program changes that are sent to the device", true, {"60", "0-127", "30,60,90"});

		registerArgument("pause-before", m_config.pauseBefore, "Pause time in seconds before the next note is being recorded. During this time, program changes are sent, if applicable", true, {"1.0"});
		registerArgument("pause-after", m_config.pauseAfter, "Additional pause time in seconds after release has finished.", true, {"1.0"});
		registerArgument("sustain-time", m_config.sustainLength, "Specify how many seconds a note is held down before released.", true, {"3.5"});
		registerArgument("release-time", m_config.releaseLength, "Specify how many seconds recording is continued after a note has been released.", true, {"3.5"});
		registerArgument("release-velocity", m_config.releaseVelocity, "Release velocity that is sent to the device when a note is released.", true, {"3.5"});
		registerArgument("midi-channel", m_config.midiChannel, "The MIDI channel that events are sent on. Range 0-15", true, {"0","15"});
		registerArgument("noisefloor-duration", m_config.detectNoisefloorDuration, "Noise floor is detected after program start, used to trim  wave files to remove silence before/after the recording of a note. Specify the duration of noise floor detected here.", true, {"3.0","5"});

		registerArgument("filename", m_config.filename, "Specify the filename that is used to create a recording. Some variables can be used to customize the file name and the path:\n "
			"{note} Note number in range 0-127\n "
			"{key} Note a human readable string like C#4. F#3, range is C-2 to G8\n "
			"{velocity} Velocity in range 0-127\n "
			"{program} Program change in range 0-127"
			, true, {"~/autosampler/device/patch{program}/{note}_{key}_{velocity}.wav"});

		registerArgument("skip-existing", m_config.skipExistingFiles, "Skip existing files that already exist on disk.", true, {"1","0"});

		// further validation
		if(m_config.filename.empty())
			throw std::exception("Filename must not be empty");

		for (auto note : m_config.noteNumbers)
		{
			if(note > 127)
				throw std::exception("Notes must be in range 0-127");	
		}

		for (auto v : m_config.velocities)
		{
			if(v > 127)
				throw std::exception("Velocity values must be in range 0-127");
		}

		for (auto p : m_config.programChanges)
		{
			if(p > 127)
				throw std::exception("Program changes must be in range 0-127");
		}
	}
	catch (const std::exception& e)
	{
		printUsage();
		LOG("command line argument error: " << e.what());
		return asLib::ErrMax;
	}

	printUsage();
	/*
	m_config.inputHostApi = "Windows DirectSound";
	m_config.inputDevice = "Eingang (High Definition Audio Device)";

	m_config.midiOutputDevice = "MIDIOUT2 (BCR2000)";

	for(uint8_t n=asLib::Note_C0; n<=asLib::Note_D0; ++n)
		m_config.noteNumbers.push_back(n);

	m_config.detectNoisefloorDuration = 5.0f;

	m_config.pauseBefore = 0.2f;
	m_config.pauseAfter = 0.2f;
	m_config.sustainLength = 3.0f;
	m_config.releaseLength = 2.0f;

	m_config.velocities.push_back(127);
	m_config.programChanges.push_back(96);
	m_config.programChanges.push_back(28);

	m_config.filename = "F:\\Samples\\Instruments\\U-110\\P{program}_V{velocity}\\{note}_{key}.wav";
	*/
	try
	{
		asLib::AutoSampler autosampler(m_config);

		autosampler.run();

		return 0;
	}
	catch (const asLib::Error& e)
	{
		LOG("Error " << e.getErrorType() << ": " << e.what())
		std::cerr << "Error " << e.getErrorType() << ": " << e.what() << std::endl;
		return e.getErrorType();
	}
}

template <> std::vector<uint8_t> Cli::parse<std::vector<uint8_t>>(const std::string& _input)
{
	std::vector<uint8_t> target;

	std::istringstream ssCommas(_input);

	std::string sCommas;

	while(std::getline(ssCommas,sCommas,','))
	{	
		std::istringstream ssSemis(sCommas);
		std::string sSemis;
		while(std::getline(ssSemis,sSemis,';'))
		{	
			std::istringstream ssMinus(sSemis);
			std::string sMinus;

			std::vector<uint8_t> tempArgs;
			while(std::getline(ssMinus,sMinus,'-'))
			{
				int arg;
				std::stringstream ssArg(sMinus);
				ssArg >> arg;
				tempArgs.push_back(static_cast<uint8_t>(arg));
			}
			if(tempArgs.empty())
				continue;	// be nice
			if(tempArgs.size() == 1)
				target.push_back(tempArgs[0]);
			else if(tempArgs.size() == 2)
			{
				const auto start = std::min(tempArgs[0], tempArgs[1]);
				const auto end = std::max(tempArgs[0], tempArgs[1]);
				if(start == end)
					target.push_back(start);
				else
					for(auto i=start; i<=end; ++i)
						target.push_back(i);
			}
			else
			{
				// but don't be nice here
				throw std::exception((std::string("Invalid argument ") + sSemis + ", expected range in form x-y").c_str());
			}
		}
	}
	
	return target;
}

void Cli::printUsage()
{
	std::cout
	<< 	"Autosampler can create multisamples of hardware MIDI devices." << std::endl
	<< "It opens a MIDI port to send notes and an Audio input to record audio data." << std::endl
	<< std::endl
	<< "Audio data is automatically cut & trimmed and written to folders according to " << std::endl
	<< "the 'filename' scheme (see below)." << std::endl
	<< std::endl
	<< "Usage:" << std::endl
	<< std::endl
	<< "All arguments need to be specified in form -arg value" << std::endl
	<< std::endl
	<< "Possible arguments:"
	<< std::endl;

	size_t longestArg = 0;
	for (const auto& arg : m_arguments)
		longestArg = std::max(arg.name.length(), longestArg);

	longestArg += 3;

	for (const auto& arg : m_arguments)
	{
		std::string line(arg.name);

		std::istringstream ss(arg.desc);

		std::string word;

		const auto minLineLength = longestArg;
		const size_t maxLineLength = 80;

		auto extendLine = [&]
		{
			while(line.length() < minLineLength)
				line += ' ';
		};

		while(getline(ss,word, ' '))
		{
			extendLine();

			if(line.length() > minLineLength)
				line += ' ';
			line += word;

			if(line.length() >= maxLineLength || line.back() == '\n')
			{
				std::cout << line << std::endl;
				line.clear();
			}
		}

		if(!line.empty())
		{
			extendLine();
			std::cout << line << std::endl;
		}

		if(!arg.defaultValue.empty())
		{
			for(size_t i=0; i<longestArg; ++i)
				std::cout << ' ';
			std::cout << "Default: " << arg.defaultValue << std::endl;
		}
		if(!arg.examples.empty())
		{
			for(size_t i=0; i<longestArg; ++i)
				std::cout << ' ';

			if(arg.examples.size() == 1)
				std::cout << "Example: " << arg.examples[0] << std::endl;
			else
			{
				std::cout << "Examples: ";
				for(size_t i=0; i<arg.examples.size(); ++i)
				{
					if(i > 0)
						std::cout << " / ";
					std::cout << arg.examples[i];
				}
				std::cout << std::endl;
			}
		}

		std::cout << std::endl;
	}

	for (const auto& arg : m_arguments)
		longestArg = std::max(arg.name.length(), longestArg);
}


}
