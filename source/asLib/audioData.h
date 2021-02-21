#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace asLib
{
	class AudioData
	{
	public:
		AudioData(unsigned long _sampleFormat, size_t _channelCount) : m_format(_sampleFormat), m_channelCount(_channelCount)
		{
		}
		AudioData(const AudioData&) = delete;

		void append(const void* _data, size_t _lengthInFrames);
		bool removeAt(size_t _frame, size_t _count);
		float floatValue(size_t _frame, size_t _channel) const;

		void trimStart(float _maxValue);
		void trimEnd(float _maxValue);

		bool empty() const					{ return m_buffer.empty(); }
		void clear()						{ m_buffer.clear(); }
		void reserve(size_t _frameCount)	{ m_buffer.reserve(bytesPerFrame() * _frameCount); }

		size_t bytesPerSample() const;
		size_t bytesPerFrame() const		{ return bytesPerSample() * m_channelCount; }
		size_t lengthInFrames() const		{ return m_buffer.size() / bytesPerFrame(); }
		size_t getChannelCount() const		{ return m_channelCount; }

		AudioData* clone();

		AudioData& operator = (const AudioData&) = delete;
		int getBitsPerSample() const;
		const std::vector<uint8_t>& data() const	{ return m_buffer; }
		bool getIsFloat() const;

	private:
		std::vector<uint8_t> m_buffer;
		const unsigned long m_format;
		const size_t m_channelCount;
	};
}
