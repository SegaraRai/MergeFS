#pragma once

#include <memory>

#include "AudioSource.hpp"
#include "Source.hpp"


std::shared_ptr<AudioSource> TransformToAudioSource(std::shared_ptr<Source> source);
