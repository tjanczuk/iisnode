#include "precomp.h"

GLOBAL_NOTIFICATION_STATUS
CNodeGlobalModule::OnGlobalConfigurationChange(
        IN IGlobalConfigurationChangeProvider * pProvider
    )
{
    UNREFERENCED_PARAMETER( pProvider );
    
    // Retrieve the path that has changed.
    PCWSTR pwszChangePath = pProvider->GetChangePath();
    
    // Test for an error.
    if (NULL != pwszChangePath && 
        wcsicmp(pwszChangePath, L"MACHINE") != 0 && 
        wcsicmp(pwszChangePath, L"MACHINE/WEBROOT") != 0)
    {
        this->pNodeApplicationManager->RecycleApplicationOnConfigChange(pwszChangePath);
    }

    // Return processing to the pipeline.
    return GL_NOTIFICATION_CONTINUE;
}