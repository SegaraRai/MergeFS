#include <cstring>
#include <utility>

#include <Windows.h>
#include <Propidl.h>

#include "PropVariantWrapper.hpp"



PropVariantWrapper::PropVariantWrapper() : PROPVARIANT{} {
  vt = VT_EMPTY;
}


PropVariantWrapper::PropVariantWrapper(PropVariantWrapper&& other) {
  /*
  std::memcpy(static_cast<PROPVARIANT*>(this), static_cast<PROPVARIANT*>(&other), sizeof(PROPVARIANT));
  std::memset(static_cast<PROPVARIANT*>(&other), 0, sizeof(PROPVARIANT));
  other.vt = VT_EMPTY;
  /*/
  *this = std::move(other);
  //*/
}


PropVariantWrapper& PropVariantWrapper::operator=(PropVariantWrapper&& other) {
  std::memcpy(static_cast<PROPVARIANT*>(this), static_cast<PROPVARIANT*>(&other), sizeof(PROPVARIANT));
  std::memset(static_cast<PROPVARIANT*>(&other), 0, sizeof(PROPVARIANT));
  other.vt = VT_EMPTY;
  return *this;
}


PropVariantWrapper::~PropVariantWrapper() {
  PropVariantClear(this);
}
