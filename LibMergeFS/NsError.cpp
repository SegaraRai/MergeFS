#include "../dokan/dokan/dokan.h"

#include "NsError.hpp"

#include <stdexcept>
#include <string>

using namespace std::literals;



NsError::NsError(NTSTATUS status, const char* message) :
  std::runtime_error(message),
  status(status)
{}


NsError::NsError(NTSTATUS status) :
  std::runtime_error("NsError"),
  status(status)
{}


NTSTATUS NsError::GetStatus() const {
  return status;
}


NsError::operator NTSTATUS() const {
  return status;
}



W32Error::W32Error(DWORD error) :
  NsError(DokanNtStatusFromWin32(error), "W32Error"),
  error(error)
{}


DWORD W32Error::GetError() const {
  return error;
}
