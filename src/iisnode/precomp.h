#ifndef __PRECOMP_H__
#define __PRECOMP_H__

// Include fewer Windows headers 
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <tchar.h>
#include <process.h>
#include <http.h>
#include <Rpc.h>
#include <queue>
#include <list>
#include <Winmeta.h>
#include <evntprov.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Psapi.h>

#include "version_autogenerated.h" // this is generated in the pre-build step

//  IIS7 Server API header file
#include "httpserv.h"

//  Project header files
#include "errors.h"
#include "utils.h"
#include "cconnectionpool.h"
#include "cnodeeventprovider.h"
#include "cnodeconstants.h"
#include "casyncmanager.h"
#include "cmoduleconfiguration.h"
#include "cnodehttpmodule.h"
#include "cnodeglobalmodule.h"
#include "cnodehttpmodulefactory.h"
#include "cactiverequestpool.h"
#include "chttpprotocol.h"
#include "cnodeapplication.h"
#include "cnodeapplicationmanager.h"
#include "cnodeprocess.h"
#include "cnodeprocessmanager.h"
#include "cprotocolbridge.h"
#include "cnodehttpstoredcontext.h"
#include "cfilewatcher.h"
#include "cnodedebugger.h"

#pragma comment(lib, "ws2_32.lib")

#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS) 0x00000000L)
#endif

typedef ULONG (NTAPI *RtlNtStatusToDosError) (NTSTATUS Status);

#endif

