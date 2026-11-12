// Compiles the vendored NativeEthernet; see Common/NativeEthernet/README.md.
// The extension host (VM_HOST_PROFILE) is built from this tree and links no Ethernet.
#ifndef VM_HOST_PROFILE
#include "../../Common/NativeEthernet/src/Nativesocket.cpp"
#endif
