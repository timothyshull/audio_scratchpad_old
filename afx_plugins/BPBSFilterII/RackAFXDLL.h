//-----------------------------------------------------------------------------
// RackAFXDLL.h
//
// Interface definitions
//-----------------------------------------------------------------------------
#include "windows.h"
#include "BPBSFilterII.h" // ***
#include "RAFXPlugInContainer.h"

#define DllExportVST3 __declspec(dllexport)
#define DllExport extern "C" __declspec(dllexport)

DllExport CPlugIn *createObject();
DllExport UINT getAPIVersion();

int g_nGUIXMLSize = 0;
char *g_pGUIXML[1] = {""};


