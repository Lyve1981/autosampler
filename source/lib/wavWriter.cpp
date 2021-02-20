#include "wavWriter.h"

#include "../base/logging.h"

#include <cstring>
#include <map>
#include <cassert>

namespace autosampler
{
bool WavWriter::write(const std::string & _filename, const std::vector<uint8_t>& data, int _bitsPerSample, bool _isFloat, int _channelCount, int _samplerate, std::vector<CuePoint>* _cuePoints /*= nullptr*/)
{
	FILE* handle = fopen(_filename.c_str(), "wb");

	if (!handle)
	{
		LOG("Failed to open file for writing: " << _filename);
		return false;
	}

	SWaveFormatHeader header;

	header.str_riff[0] = 'R';
	header.str_riff[1] = 'I';
	header.str_riff[2] = 'F';
	header.str_riff[3] = 'F';

	header.str_wave[0] = 'W';
	header.str_wave[1] = 'A';
	header.str_wave[2] = 'V';
	header.str_wave[3] = 'E';

	const size_t dataSize = data.size();

	header.file_size = 
		sizeof(SWaveFormatHeader) + 
		sizeof(SWaveFormatChunkInfo) + 
		sizeof(SWaveFormatChunkFormat) +
		sizeof(SWaveFormatChunkInfo) +
		dataSize +
		- 8;

	fwrite(&header, 1, sizeof(header), handle);

	// write format
	SWaveFormatChunkInfo chunkInfo;

	chunkInfo.chunkName[0] = 'f';
	chunkInfo.chunkName[1] = 'm';
	chunkInfo.chunkName[2] = 't';
	chunkInfo.chunkName[3] = ' ';

	chunkInfo.chunkSize = sizeof(SWaveFormatChunkFormat);

	fwrite(&chunkInfo, 1, sizeof(chunkInfo), handle);

	SWaveFormatChunkFormat fmt;

	const auto bytesPerSample = _bitsPerSample >> 3;
	const auto bytesPerFrame = bytesPerSample * _channelCount;

	fmt.bits_per_sample = _bitsPerSample;
	fmt.block_alignment = bytesPerFrame;
	fmt.bytes_per_sec = _samplerate * bytesPerFrame;
	fmt.num_channels = (uint16_t)_channelCount;
	fmt.sample_rate = (unsigned int)_samplerate;
	fmt.wave_type = _isFloat ? eFormat_IEEE_FLOAT : eFormat_PCM;

	fwrite(&fmt, 1, sizeof(fmt), handle);

	// write data
	chunkInfo.chunkName[0] = 'd';
	chunkInfo.chunkName[1] = 'a';
	chunkInfo.chunkName[2] = 't';
	chunkInfo.chunkName[3] = 'a';

	chunkInfo.chunkSize = static_cast<unsigned int>(data.size());

	fwrite(&chunkInfo, 1, sizeof(chunkInfo), handle);

	fwrite(&data[0], 1, data.size(), handle);

	if(_cuePoints && !_cuePoints->empty())
	{
		// write cue points
		chunkInfo.chunkName[0] = 'c';
		chunkInfo.chunkName[1] = 'u';
		chunkInfo.chunkName[2] = 'e';
		chunkInfo.chunkName[3] = ' ';
		chunkInfo.chunkSize = sizeof(SWaveFormatChunkCue) + sizeof(SWaveFormatChunkCuePoint) * _cuePoints->size();

		header.file_size += sizeof(SWaveFormatChunkInfo) + chunkInfo.chunkSize;

		fwrite(&chunkInfo, 1, sizeof(chunkInfo), handle);

		SWaveFormatChunkCue chunkCue;
		chunkCue.cuePointCount = _cuePoints->size();

		fwrite(&chunkCue, 1, sizeof(chunkCue), handle);

		for (size_t i = 0; i < chunkCue.cuePointCount; ++i)
		{
			SWaveFormatChunkCuePoint point;
			point.cueId = i;
			point.blockStart = 0;
			point.chunkStart = 0;
			point.dataChunkId[0] = 'd';
			point.dataChunkId[1] = 'a';
			point.dataChunkId[2] = 't';
			point.dataChunkId[3] = 'a';
			point.sampleOffset = (*_cuePoints)[i].sampleOffset;
			point.playOrderPosition = point.sampleOffset;

			fwrite(&point, 1, sizeof(point), handle);
		}

		// write cue point labels
		chunkInfo.chunkName[0] = 'L';
		chunkInfo.chunkName[1] = 'I';
		chunkInfo.chunkName[2] = 'S';
		chunkInfo.chunkName[3] = 'T';
		chunkInfo.chunkSize = sizeof(SWaveFormatChunkList);

		SWaveFormatChunkList adtl;
		adtl.typeId[0] = 'a';
		adtl.typeId[1] = 'd';
		adtl.typeId[2] = 't';
		adtl.typeId[3] = 'l';

		std::vector<uint8_t> buffer;

		for(size_t i=0; i<_cuePoints->size(); ++i)
		{
			const CuePoint& cuePoint = (*_cuePoints)[i];

			SWaveFormatChunkInfo subChunkInfo;

			subChunkInfo.chunkName[0] = 'l';
			subChunkInfo.chunkName[1] = 'a';
			subChunkInfo.chunkName[2] = 'b';
			subChunkInfo.chunkName[3] = 'l';

			subChunkInfo.chunkSize = sizeof(SWaveFormatChunkLabel) + cuePoint.name.size() + 1;

			SWaveFormatChunkLabel label;
			label.cuePointId = i;

			size_t writePos = buffer.size();
			const size_t totalCuePointSize = sizeof(SWaveFormatChunkInfo) + ((subChunkInfo.chunkSize + 1) & ~1);
			buffer.resize(writePos + totalCuePointSize, 0); 

			::memcpy(&buffer[writePos], &subChunkInfo, sizeof(subChunkInfo));
			writePos += sizeof(subChunkInfo);
			::memcpy(&buffer[writePos], &label, sizeof(label));
			writePos += sizeof(label);
			::memcpy(&buffer[writePos], cuePoint.name.c_str(), cuePoint.name.size());
		}

		chunkInfo.chunkSize += buffer.size();

		fwrite(&chunkInfo, 1, sizeof(chunkInfo), handle);
		fwrite(&adtl, 1, sizeof(adtl), handle);
		fwrite(&buffer[0], 1, buffer.size(), handle);

		header.file_size += sizeof(SWaveFormatChunkInfo) + chunkInfo.chunkSize;
	}

	fseek(handle, 0, SEEK_SET);

	fwrite(&header, 1, sizeof(header), handle);

	fclose(handle);

	return true;
}

}