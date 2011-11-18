#ifndef __CNODEDEBUGGER_H__
#define __CNODEDEBUGGER_H__

class CNodeEventProvider;
class CNodeHttpStoredContext;

enum NodeDebugCommand {
	ND_NONE,
	ND_DEBUG,
	ND_DEBUG_BRK,
	ND_KILL,
	ND_REDIRECT
};

class CNodeDebugger
{
public:

	static HRESULT GetDebugCommand(IHttpContext* context, CNodeEventProvider* log, NodeDebugCommand* debugCommand);
	static HRESULT DispatchDebuggingRequest(CNodeHttpStoredContext* ctx, BOOL* requireChildContext, BOOL* mainDebuggerPage);

};

#endif