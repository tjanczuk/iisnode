#ifndef __CNODEHTTPSTOREDCONTEXT_H__
#define __CNODEHTTPSTOREDCONTEXT_H__

class CNodeHttpStoredContext : public IHttpStoredContext
{
private:

	IHttpContext* context;

public:

	// Context is owned by the caller
	CNodeHttpStoredContext(IHttpContext* context);
	~CNodeHttpStoredContext();

	IHttpContext* GetHttpContext() { return this->context; }

	virtual void CleanupStoredContext();
};

#endif