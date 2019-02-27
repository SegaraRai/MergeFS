#pragma once

#include <winrt/base.h>


template<typename T>
winrt::com_ptr<T> CreateCOMPtr(T* ptr) {
  winrt::com_ptr<T> comPtr;
  comPtr.attach(ptr);
  return comPtr;
}
