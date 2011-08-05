#ifndef __CNODEHTTPMODULEFACTORY_H__
#define __CNODEHTTPMODULEFACTORY_H__

class CNodeHttpModuleFactory : public IHttpModuleFactory
{
	CNodeApplicationManager* applicationManager;


public:

	CNodeHttpModuleFactory(); 
	~CNodeHttpModuleFactory();
	HRESULT Initialize(IHttpServer* server, HTTP_MODULE_ID moduleId);
    virtual HRESULT GetHttpModule(OUT CHttpModule **ppModule, IN IModuleAllocator *);
    virtual void Terminate();
};

#endif
