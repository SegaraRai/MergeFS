#pragma once

#include "../dokan/dokan/dokan.h"

#include "Mount.hpp"


extern const DOKAN_OPERATIONS gDokanOperations;


ULONG64 GetGlobalContextFromMount(Mount* mount);
