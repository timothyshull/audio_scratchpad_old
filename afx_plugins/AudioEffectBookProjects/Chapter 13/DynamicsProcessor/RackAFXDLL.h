//-----------------------------------------------------------------------------
// RackAFXDLL.h
//
// Interface definitions
//-----------------------------------------------------------------------------
#include "windows.h"
#include "DynamicsProcessor.h" // ***

#define DllExport extern "C" __declspec(dllexport)

DllExport CPlugIn *createObject();
DllExport UINT getAPIVersion();

