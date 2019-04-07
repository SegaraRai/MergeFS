#pragma once

#include <Windows.h>
#include <Propidl.h>


class PropVariantWrapper : public PROPVARIANT {
public:
  PropVariantWrapper();

  PropVariantWrapper(const PropVariantWrapper&) = delete;
  PropVariantWrapper& operator=(const PropVariantWrapper&) = delete;

  PropVariantWrapper(PropVariantWrapper&& other) noexcept;
  PropVariantWrapper& operator=(PropVariantWrapper&& other) noexcept;

  ~PropVariantWrapper();
};
