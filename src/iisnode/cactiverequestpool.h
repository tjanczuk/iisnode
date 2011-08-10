#ifndef __CACTIVEREQUESTPOOL_H__
#define __CACTIVEREQUESTPOOL_H__

class CNodeHttpStoredContext;

typedef std::list<CNodeHttpStoredContext*> CNodeHttpStoredContextList;

class CActiveRequestPool
{
private: 
	
	CNodeHttpStoredContextList requests;
	CRITICAL_SECTION syncRoot;

public:

	CActiveRequestPool();
	~CActiveRequestPool();

	HRESULT Add(CNodeHttpStoredContext* context);
	HRESULT Remove(CNodeHttpStoredContext* context);
};

#endif