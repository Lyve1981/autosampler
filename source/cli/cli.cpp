#include <iostream>

#include "../base/logging.h"

#include "../lib/autosampler.h"
#include "../lib/config.h"
#include "../lib/error.h"
#include "../lib/midiTypes.h"

int main(int argc, char* argv[])
{
	try
	{
		asLib::Config config;

		config.inputHostApi = "Windows DirectSound";
		config.inputDevice = "Eingang (High Definition Audio Device)";

		config.midiOutputDevice = "MIDIOUT2 (BCR2000)";

		for(uint8_t n=asLib::Note_C0; n<=asLib::Note_D0; ++n)
			config.noteNumbers.push_back(n);

		config.detectNoisefloorDuration = 5.0f;

		config.pauseBefore = 0.2f;
		config.pauseAfter = 0.2f;
		config.sustainLength = 3.0f;
		config.releaseLength = 2.0f;

		config.velocities.push_back(127);
		config.programChanges.push_back(96);
		config.programChanges.push_back(28);

		config.filename = "F:\\Samples\\Instruments\\U-110\\P{program}_V{velocity}\\{note}_{key}.wav";

		asLib::AutoSampler autosampler(config);

		autosampler.run();

		return 0;
	}
	catch (const Error& e)
	{
		LOG("Error " << e.getErrorType() << ": " << e.what())
		std::cerr << "Error " << e.getErrorType() << ": " << e.what() << std::endl;
		return e.getErrorType();
	}
}
