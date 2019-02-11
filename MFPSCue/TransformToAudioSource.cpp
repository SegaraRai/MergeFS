#include <memory>

#include "TransformToAudioSource.hpp"
#include "AudioSource.hpp"
#include "Source.hpp"
#include "SourceToAudioSourceBin.hpp"
#include "SourceToAudioSourceFLAC.hpp"
#include "SourceToAudioSourceWAV.hpp"



namespace {
  template<typename T, typename ...Ts>
  std::shared_ptr<AudioSource> GetAudioSource(std::shared_ptr<Source> source) {
    try {
      return std::make_shared<T>(source);
    } catch (...) {}
    if constexpr (sizeof...(Ts) > 0) {
      return GetAudioSource<Ts...>(source);
    }
    return nullptr;
  }
}


std::shared_ptr<AudioSource> TransformToAudioSource(std::shared_ptr<Source> source, bool enableBin) {
  if (enableBin) {
    auto audioSource = GetAudioSource<SourceToAudioSourceBin>(source);
    if (audioSource) {
      return audioSource;
    }
  }
  return GetAudioSource<SourceToAudioSourceFLAC, SourceToAudioSourceWAV>(source);
}
