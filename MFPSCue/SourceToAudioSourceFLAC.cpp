#define NOMINMAX

#include <dokan/dokan.h>

#include <FLAC++/decoder.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>

#include "SourceToAudioSourceFLAC.hpp"
#include "AudioSource.hpp"
#include "Source.hpp"



namespace {
  // stereo 16bit
  using DataType = std::int16_t;
  constexpr std::size_t NumChannels = 2;
  constexpr std::size_t Alignment = sizeof(DataType) * NumChannels;
  constexpr bool EnableMD5Check = false;


  class FLACDecoderImpl : public FLAC::Decoder::Stream {
  public:
    using ErrorCallback = std::function<void(FLAC__StreamDecoderErrorStatus status)>;
    using MetadataCallback = std::function<void(const FLAC__StreamMetadata* metadata)>;
    using WriteCallback = std::function<FLAC__StreamDecoderWriteStatus(const FLAC__Frame* frame, const FLAC__int32* const buffer[])>;

  private:
    std::shared_ptr<Source> mSource;
    Source::SourceOffset mSize;
    Source::SourceOffset mSeekPosition;
    ErrorCallback mErrorCallback;
    MetadataCallback mMetadataCallback;
    WriteCallback mWriteCallback;

  public:
    FLACDecoderImpl(std::shared_ptr<Source> source) :
      mSource(source),
      mSize(source->GetSize()),
      mSeekPosition(0),
      mErrorCallback(),
      mMetadataCallback(),
      mWriteCallback()
    {}


    FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], std::size_t* bytes) override {
      if (mSeekPosition >= mSize) {
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
      }
      std::size_t readSize;
      if (const auto status = mSource->Read(mSeekPosition, reinterpret_cast<std::byte*>(buffer), *bytes, &readSize); status != STATUS_SUCCESS) {
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
      }
      *bytes = readSize;
      mSeekPosition += readSize;
      return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }


    FLAC__StreamDecoderSeekStatus seek_callback(FLAC__uint64 absolute_byte_offset) override {
      if (absolute_byte_offset > mSize) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
      }
      mSeekPosition = absolute_byte_offset;
      return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    }


    FLAC__StreamDecoderTellStatus tell_callback(FLAC__uint64* absolute_byte_offset) override {
      *absolute_byte_offset = static_cast<FLAC__uint64>(mSeekPosition);
      return FLAC__STREAM_DECODER_TELL_STATUS_OK;
    }


    FLAC__StreamDecoderLengthStatus length_callback(FLAC__uint64* stream_length) override {
      *stream_length = static_cast<FLAC__uint64>(mSize);
      return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
    }


    bool eof_callback() override {
      return mSeekPosition >= mSize;
    }


    FLAC__StreamDecoderWriteStatus write_callback(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) override {
      if (!mWriteCallback) {
        throw std::runtime_error(u8"no write callback provided");
      }
      return mWriteCallback(frame, buffer);
    }


    void metadata_callback(const FLAC__StreamMetadata* metadata) override {
      if (!mMetadataCallback) {
        return;
      }
      mMetadataCallback(metadata);
    }


    void error_callback(FLAC__StreamDecoderErrorStatus status) override {
      if (!mErrorCallback) {
        throw std::runtime_error(u8"no error callback provided");
      }
      mErrorCallback(status);
    }


    void SetErrorCallback(ErrorCallback errorCallback = nullptr) {
      mErrorCallback = errorCallback;
    }


    void SetMetadataCallback(MetadataCallback metadataCallback = nullptr) {
      mMetadataCallback = metadataCallback;
    }


    void SetWriteCallback(WriteCallback writeCallback = nullptr) {
      mWriteCallback = writeCallback;
    }
  };


  void CopyAudioData(std::byte* destBuffer, std::size_t size, std::size_t unalignedOffset, const FLAC__int32* sourceLeft, const FLAC__int32* sourceRight) {
    assert(unalignedOffset < Alignment);

    const auto alignedSourceIndex = unalignedOffset ? 1 : 0;
    const auto alignOffset = unalignedOffset ? Alignment - unalignedOffset : 0;
    const auto alignedNumBlocks = size > alignOffset ? (size - alignOffset) / Alignment : 0;

    {
      auto alignedBuffer = reinterpret_cast<DataType*>(destBuffer + alignOffset);
      auto ptrEnd = alignedBuffer + alignedNumBlocks * NumChannels;
      auto ptr = alignedBuffer;
      auto ptrLeft = sourceLeft + alignedSourceIndex;
      auto ptrRight = sourceRight + alignedSourceIndex;
      while (ptr != ptrEnd) {
        *ptr++ = static_cast<DataType>(*ptrLeft++);
        *ptr++ = static_cast<DataType>(*ptrRight++);
      }
    }

    // beginning piece
    if (unalignedOffset) {
      auto ptr = destBuffer;
      switch (alignOffset) {
        case 3:
          // upper byte of L
          *ptr++ = static_cast<std::byte>((sourceLeft[0] >> 8) & 0xFF);
          [[fallthrough]];
        case 2:
          // lower byte of R
          *ptr++ = static_cast<std::byte>(sourceRight[0] & 0xFF);
          [[fallthrough]];
        case 1:
          // upper byte of R
          *ptr++ = static_cast<std::byte>((sourceRight[0] >> 8) & 0xFF);
      }
    }

    // ending piece
    if (size > alignOffset && (size - alignOffset) % Alignment) {
      auto ptr = destBuffer + size;
      const auto sourceIndex = alignedSourceIndex + alignedNumBlocks;
      switch ((size - alignOffset) % Alignment) {
        case 3:
          // lower byte of R
          *--ptr = static_cast<std::byte>(sourceRight[sourceIndex] & 0xFF);
          [[fallthrough]];
        case 2:
          // upper byte of L
          *--ptr = static_cast<std::byte>((sourceLeft[sourceIndex] >> 8) & 0xFF);
          [[fallthrough]];
        case 1:
          // lower byte of L
          *--ptr = static_cast<std::byte>(sourceLeft[sourceIndex] & 0xFF);
      }
    }
  }


  std::size_t CopyAudioFromFLACBlock(const FLAC__Frame& frame, const FLAC__int32* const* sourceBuffer, std::byte* destinationBuffer, Source::SourceOffset currentByteOffset, Source::SourceOffset requestedByteEnd) {
    const auto firstSampleIndex = frame.header.number.sample_number;
    const auto numSamplesInBlock = frame.header.blocksize;
    const auto lastSampleIndex = firstSampleIndex + numSamplesInBlock;

    const auto currentFirstSampleForBuffer = currentByteOffset / Alignment;
    const auto copyStartSampleOffset = currentFirstSampleForBuffer - firstSampleIndex;
    const auto copySize = std::min(lastSampleIndex * Alignment - currentByteOffset, requestedByteEnd - currentByteOffset);
    CopyAudioData(destinationBuffer, copySize, currentByteOffset % Alignment, sourceBuffer[0] + copyStartSampleOffset, sourceBuffer[1] + copyStartSampleOffset);

    return copySize;
  }
}


SourceToAudioSourceFLAC::SourceToAudioSourceFLAC(std::shared_ptr<Source> source) :
  mError(false),
  mDecoder(std::make_shared<FLACDecoderImpl>(source)),
  mLastFLACFrame(std::make_unique<std::byte[]>(sizeof(FLAC__Frame))),
  mDataType(DataType::Other),
  mTotalSamples(0),
  mTotalSize(0)
{
  auto& decoder = *static_cast<FLACDecoderImpl*>(mDecoder.get());
  decoder.SetErrorCallback([this](FLAC__StreamDecoderErrorStatus errorStatus) {
    mError = true;
  });
  decoder.SetMetadataCallback([this, &decoder](const FLAC__StreamMetadata* metadata) {
    if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
      return;
    }
    if (mTotalSamples) {
      // too many STREAMINFO; error
      return;
    }
    const auto& samplingRate = metadata->data.stream_info.sample_rate;
    const auto& channels = metadata->data.stream_info.channels;
    const auto& bitsPerSample = metadata->data.stream_info.bits_per_sample;
    const auto& totalSamples = metadata->data.stream_info.total_samples;
    mChannelInfo.resize(channels);
    if (mChannelInfo.size() == 2) {
      mChannelInfo[0] = ChannelInfo::Left;
      mChannelInfo[1] = ChannelInfo::Right;
    } else {
      for (std::size_t channelIndex = 0; channelIndex < mChannelInfo.size(); channelIndex++) {
        mChannelInfo[channelIndex] = ChannelInfo::Other;
      }
    }
    mDataType = bitsPerSample == 16 ? DataType::Int16 : DataType::Other;
    mSamplingRate = samplingRate;
    mTotalSamples = totalSamples;
    mTotalSize = totalSamples * (bitsPerSample / 8) * channels;
  });
  if constexpr (EnableMD5Check) {
    decoder.set_md5_checking(true);
  }
  if (decoder.init() != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    throw std::runtime_error(u8"failed to initialize FLAC stream decoder");
  }
  if (!decoder.process_until_end_of_metadata()) {
    throw std::runtime_error(u8"failed to process metadata");
  }
  if (mError) {
    throw std::runtime_error(u8"an error occurred while decoding stream");
  }
  if (!mTotalSamples) {
    throw std::runtime_error(u8"unsupported FLAC file");
  }
  decoder.SetMetadataCallback();
}


std::size_t SourceToAudioSourceFLAC::GetChannels() const {
  return mChannelInfo.size();
}


AudioSource::ChannelInfo SourceToAudioSourceFLAC::GetChannelInfo(std::size_t channelIndex) const {
  if (channelIndex >= mChannelInfo.size()) {
    return ChannelInfo::None;
  }
  return mChannelInfo[channelIndex];
}


AudioSource::DataType SourceToAudioSourceFLAC::GetDataType() const {
  return mDataType;
}


std::uint_fast32_t SourceToAudioSourceFLAC::GetSamplingRate() const {
  return mSamplingRate;
}


Source::SourceSize SourceToAudioSourceFLAC::GetSize() {
  return mTotalSize;
}


NTSTATUS SourceToAudioSourceFLAC::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  if (mChannelInfo.size() != NumChannels || mDataType != DataType::Int16) {
    return STATUS_UNSUCCESSFUL;
  }

  if (!size) {
    if (readSize) {
      *readSize = 0;
    }
    return STATUS_SUCCESS;
  }

  std::lock_guard lock(mMutex);

  const auto requestedByteEnd = offset + size;
  const auto unalignedOffset = offset % Alignment;
  const auto requestedSampleBegin = offset / Alignment;

  bool needsSeek = true;
  if (mLastFLACAvailable) {
    const auto& lastFLACFrame = *reinterpret_cast<FLAC__Frame*>(mLastFLACFrame.get());
    const auto firstSampleIndex = lastFLACFrame.header.number.sample_number;
    const auto numSamplesInBlock = lastFLACFrame.header.blocksize;
    const auto lastSampleIndex = firstSampleIndex + numSamplesInBlock;

    if (firstSampleIndex <= requestedSampleBegin && requestedSampleBegin < lastSampleIndex) {
      needsSeek = false;
    }
  }

  auto& decoder = *static_cast<FLACDecoderImpl*>(mDecoder.get());
  if (mError) {
    decoder.flush();
    if constexpr (EnableMD5Check) {
      decoder.set_md5_checking(true);
    }
    mError = false;
  }
  auto currentByteOffset = offset;
  auto currentBufferPointer = buffer;
  bool finished = false;
  bool outOfRange = false;
  decoder.SetWriteCallback([this, &currentByteOffset, &currentBufferPointer, &finished, &outOfRange, requestedByteEnd](const FLAC__Frame* frame, const FLAC__int32* const srcBuffer[]) -> FLAC__StreamDecoderWriteStatus {
    if (frame->header.channels != mChannelInfo.size() || frame->header.sample_rate != mSamplingRate || frame->header.bits_per_sample != 16) {
      // format changed?
      finished = true;
      return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    const auto firstSampleIndex = frame->header.number.sample_number;
    const auto numSamplesInBlock = frame->header.blocksize;
    const auto lastSampleIndex = firstSampleIndex + numSamplesInBlock;

    const auto currentSampleOffset = currentByteOffset / Alignment;
    if (firstSampleIndex > currentSampleOffset || currentSampleOffset >= lastSampleIndex) {
      // out of range
      if (firstSampleIndex > currentSampleOffset) {
        outOfRange = true;
      }
      return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

    const auto copiedSize = CopyAudioFromFLACBlock(*frame, srcBuffer, currentBufferPointer, currentByteOffset, requestedByteEnd);
    currentBufferPointer += copiedSize;
    currentByteOffset += copiedSize;
    finished = currentByteOffset == requestedByteEnd;

    if (finished) {
      // copy FLAC data for next use
      mLastFLACAvailable = true;
      *reinterpret_cast<FLAC__Frame*>(mLastFLACFrame.get()) = *frame;
      std::size_t blockSize = frame->header.blocksize * sizeof(FLAC__int32);
      mLastFLACBufferData = std::make_unique<std::byte[]>(blockSize * frame->header.channels);
      for (std::size_t i = 0; i < frame->header.channels; i++) {
        mLastFLACBufferPointers[i] = mLastFLACBufferData.get() + blockSize * i;
        std::memcpy(mLastFLACBufferPointers[i], srcBuffer[i], blockSize);
      }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
  });
  if (needsSeek) {
    if (!decoder.seek_absolute(requestedSampleBegin)) {
      return __NTSTATUS_FROM_WIN32(ERROR_SEEK);
    }
  } else {
    // mLastFLACAvailable is true if needsSeek is false
    const auto copiedSize = CopyAudioFromFLACBlock(*reinterpret_cast<FLAC__Frame*>(mLastFLACFrame.get()), reinterpret_cast<const FLAC__int32* const*>(mLastFLACBufferPointers), currentBufferPointer, currentByteOffset, requestedByteEnd);
    currentBufferPointer += copiedSize;
    currentByteOffset += copiedSize;
    finished = currentByteOffset == requestedByteEnd;
  }
  bool isFirstOutOfRange = needsSeek;
  while (!finished) {
    if (!decoder.process_single()) {
      return STATUS_UNSUCCESSFUL;
    }
    if (outOfRange) {
      if (isFirstOutOfRange) {
        isFirstOutOfRange = false;
      } else {
        return STATUS_UNSUCCESSFUL;
      }
    }
    if (mError) {
      return STATUS_UNSUCCESSFUL;
    }
    switch (decoder.get_state()) {
      case FLAC__STREAM_DECODER_OGG_ERROR:
      case FLAC__STREAM_DECODER_SEEK_ERROR:
      case FLAC__STREAM_DECODER_ABORTED:
      case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
      case FLAC__STREAM_DECODER_UNINITIALIZED:
        return STATUS_UNSUCCESSFUL;
    }
  };

  if (readSize) {
    *readSize = currentByteOffset - offset;
  }

  return STATUS_SUCCESS;
}
