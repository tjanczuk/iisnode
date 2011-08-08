#ifndef __PRECOMP_H__
#define __PRECOMP_H__

// Include fewer Windows headers 
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <queue>

//  IIS7 Server API header file
#include "httpserv.h"

//  Project header files
#include "utils.h"
#include "cmoduleconfiguration.h"
#include "cnodehttpmodule.h"
#include "cnodehttpmodulefactory.h"
#include "cactiverequestpool.h"
#include "chttpprotocol.h"
#include "cnodeapplication.h"
#include "cnodeapplicationmanager.h"
#include "cnodeprocess.h"
#include "cnodeprocessmanager.h"
#include "cpendingrequestqueue.h"
#include "cprotocolbridge.h"
#include "cnodehttpstoredcontext.h"

#endif

