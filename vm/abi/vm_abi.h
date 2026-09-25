// SPDX-License-Identifier: MIT
// The extension ABI, as a module sees it: the in-tree include path for
// VMABI.h, which is the only TeensyROM header a module needs. Copy that file,
// not this one, to build out of tree -- the path below does not travel.
#pragma once
#include "../../Source/Teensy/MinimalBoot/Common/VMABI.h"
