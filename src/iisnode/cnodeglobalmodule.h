#ifndef __CNODEGLOBALMODULE_H__
#define __CNODEGLOBALMODULE_H__

class CNodeApplicationManager;

class CNodeGlobalModule : public CGlobalModule
{
private:
    CNodeApplicationManager* pNodeApplicationManager;

public:
    
    CNodeGlobalModule(CNodeApplicationManager* pNodeApplicationManager)
    {
        this->pNodeApplicationManager = pNodeApplicationManager;
    }

    ~CNodeGlobalModule()
    {
    }

    VOID Terminate()
    {
        // Remove the class from memory.
        delete this;
    }

    GLOBAL_NOTIFICATION_STATUS
    OnGlobalConfigurationChange(
        IN IGlobalConfigurationChangeProvider * pProvider
    );
};

#endif