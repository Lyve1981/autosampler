#pragma once

#include <string>
#include <vector>

namespace asLib
{
struct Config
{
	// Audio Input
	int inputSamplerate = 48000;
	int inputBits = 24;
	int inputChannels = 1;
	int inputBlockSize = 1024;
	std::string inputDevice;
	std::string inputHostApi;

	// MIDI Output
	std::string midiOutputDevice;
	std::string midiOutputApi;

	// Processing - MIDI
	std::vector<uint8_t> noteNumbers;
	std::vector<uint8_t> velocities = {127};
	std::vector<uint8_t> programChanges;
	uint8_t releaseVelocity = 0;
	uint8_t midiChannel = 0;

	// Processing - Audio
	float detectNoisefloorDuration = 2.0f;

	float pauseBefore = 0.5f;
	float sustainLength = 3.0f;
	float releaseLength = 1.0f;
	float pauseAfter = 0.5f;

	// I/O
	std::string filename = "";
	bool skipExistingFiles = true;
};
}
