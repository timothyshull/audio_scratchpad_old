#pragma once

#include "plugin.h"

#define DllExport extern "C" __declspec(dllexport)

class CRAFXPlugInContainer {
public:
    static CPlugIn *createRAFXPlugIn();

    static CRAFXPlugInContainer *getRAFXPlugInContainer();

    static void createRAFXPlugInContainer(void *hInstance, CPlugIn *(*createObject)(void),
                                          unsigned long uProcCID1, unsigned long uProcCID2, unsigned long uProcCID3,
                                          unsigned long uProcCID4,
                                          unsigned long uContCID1, unsigned long uContCID2, unsigned long uContCID3,
                                          unsigned long uContCID4,
                                          char **ppXMLFile, int nNumXMLLines, char *pCompanyName);

    static void destroyRAFXPlugInContainer();

    HMODULE m_hPluginInstance;

    CPlugIn *(*createObject)(void);

    char *m_PlugInName;
    char *m_CompanyName;
    bool m_bOutputOnlyPlugIn;

    unsigned long m_uProcCID1;
    unsigned long m_uProcCID2;
    unsigned long m_uProcCID3;
    unsigned long m_uProcCID4;
    unsigned long m_uContCID1;
    unsigned long m_uContCID2;
    unsigned long m_uContCID3;
    unsigned long m_uContCID4;

    char **m_ppXMLFile;
    int m_nNumXMLLines;
};
