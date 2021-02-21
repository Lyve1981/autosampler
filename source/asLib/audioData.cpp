#include "audioData.h"
#include "error.h"

#include <cmath>
#include <limits>
#include <memory.h>

#include "../portaudio/include/portaudio.h"

void asLib::AudioData::append(const void* _data, size_t _lengthInFrames)
{
	const auto oldSize = m_buffer.size();
	const auto appendSize = bytesPerFrame() * _lengthInFrames;
	const auto newSize = oldSize + appendSize;

	m_buffer.resize(newSize);

	::memcpy(&m_buffer[oldSize], _data, appendSize);
}

bool asLib::AudioData::removeAt(size_t _frame, size_t _count)
{
	const auto byteOffset = _frame * bytesPerFrame();
	const auto byteRemoveSize = _count * bytesPerFrame();

	auto last = byteOffset + byteRemoveSize;

	if(byteOffset >= m_buffer.size())
		return false;

	if(last >= m_buffer.size())
		last = m_buffer.size();

	m_buffer.erase(m_buffer.begin() + byteOffset, m_buffer.begin() + last);

	return true;
}

float asLib::AudioData::floatValue(size_t _frame, size_t _channel) const
{
	if(_channel >= m_channelCount)
		return 0.0f;

	const auto byteOffset = bytesPerFrame() * _frame;

	if((byteOffset + bytesPerFrame()) >= m_buffer.size())
		return 0.0f;

	const auto* first = &m_buffer[byteOffset];

	switch (m_format)
	{
	case paFloat32:
		{
			return *reinterpret_cast<const float*>(first);		
		}
	case paInt32:
		{
			const auto intValue = *reinterpret_cast<const int32_t*>(first);
			return static_cast<float>(static_cast<double>(intValue) / -static_cast<double>(std::numeric_limits<int32_t>::min()));			
		}
	case paInt24:
		{
			union ConvertHelper
			{
				int8_t buf[4];
				int32_t asInt;
			};

			ConvertHelper helper;

			helper.asInt = 0;
			helper.buf[0] = first[0];
			helper.buf[1] = first[1];
			helper.buf[2] = first[2];

			// fix sign
			helper.asInt <<= 8;
			helper.asInt >>= 8;

			return static_cast<float>(static_cast<double>(helper.asInt) / static_cast<double>(1 << 23));	
		}
	case paInt16:
		{
			const auto int16Value = *reinterpret_cast<const int16_t*>(first);
			return static_cast<float>(static_cast<double>(int16Value) / -static_cast<double>(std::numeric_limits<int16_t>::min()));			
		}
	case paInt8:
	case paUInt8:
		return static_cast<float>(static_cast<double>(*first) / -static_cast<double>(std::numeric_limits<int8_t>::min()));
	default:
		throw Error(ErrAudioInput, "Unknown stream format");
	}	
}

void asLib::AudioData::trimStart(float _maxValue)
{
	if(empty())
		return;
	
	auto found = false;

	size_t index = 0;

	for(size_t f=0; f<lengthInFrames() && !found; ++f)
	{
		for(size_t c=0; c<getChannelCount() && !found; ++c)
		{
			const float g = std::abs(floatValue(f, c));

			if(g >= _maxValue)
			{
				if(f == 0)
					return;

				found = true;
				index = f-1;

				break;				
			}
		}
	}

	if(!found)
	{
		clear();
		return;
	}
	
	const auto bytesToRemove = index * bytesPerFrame();
	if(bytesToRemove == m_buffer.size())
		clear();
	else
		m_buffer.erase(m_buffer.begin(), m_buffer.begin() + bytesToRemove);
}

void asLib::AudioData::trimEnd(float _maxValue)
{
	if(empty())
		return;

	auto found = false;

	size_t index = 0;

	for(size_t f=m_buffer.size()-1; f<std::numeric_limits<size_t>::max() && !found; --f)
	{
		for(size_t c=0; c<getChannelCount() && !found; ++c)
		{
			const float g = std::abs(floatValue(f, c));

			if(g >= _maxValue)
			{
				if(f == m_buffer.size()-1)
					return;

				found = true;
				index = f-1;

				break;				
			}
		}
	}

	if(!found)
	{
		clear();
		return;
	}

	const auto newSize = bytesPerFrame() * index;
	m_buffer.resize(newSize);
}

size_t asLib::AudioData::bytesPerSample() const
{
	return Pa_GetSampleSize(m_format);
}

asLib::AudioData* asLib::AudioData::clone()
{
	auto* clone = new AudioData(m_format, m_channelCount);
	clone->m_buffer.assign(m_buffer.begin(), m_buffer.end());
	return clone;
}

int asLib::AudioData::getBitsPerSample() const
{
	return bytesPerSample() << 3;
}

bool asLib::AudioData::getIsFloat() const
{
	return m_format == paFloat32;
}
