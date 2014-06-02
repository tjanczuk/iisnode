#ifndef __UTILS_H__
#define __UTILS_H__

#define ErrorIf(expr, hresult) if (expr) { hr = hresult; goto Error; }

#define CheckNull(expr) ErrorIf(NULL == expr, ERROR_INVALID_PARAMETER)

#define CheckError(hresult) { HRESULT tmp_hr = hresult; if (S_OK != tmp_hr) { hr = tmp_hr; goto Error; } }

#define ENTER_CS(cs) EnterCriticalSection(&cs); __try {

#define LEAVE_CS(cs) } __finally { LeaveCriticalSection(&cs); }

#define ENTER_SRW_SHARED(srw) AcquireSRWLockShared(&srw); __try {

#define LEAVE_SRW_SHARED(srw) } __finally { ReleaseSRWLockShared(&srw); }

#define ENTER_SRW_EXCLUSIVE(srw) AcquireSRWLockExclusive(&srw); __try {

#define LEAVE_SRW_EXCLUSIVE(srw) } __finally { ReleaseSRWLockExclusive(&srw); }

struct ENUM_INDEX
{
    VARIANT Index;
    ULONG Count;
};

#include <sddl.h>
#include <AccCtrl.h>
#include <Aclapi.h>

class CUtils
{
public:
    ~CUtils()
    {
    }

    static
    VOID
    CUtils::FreeTokenUserSID(
        TOKEN_USER ** ppTokenUser
        )
    {
        _ASSERT( ppTokenUser );
        if ( ppTokenUser == NULL ) 
        {
            return;
        }

        if ( *ppTokenUser != NULL )
        {
            delete [] *ppTokenUser;
            *ppTokenUser = NULL;
        }
    }

    static
    DWORD
    CUtils::GetTokenUserSID(
        HANDLE hToken,
        TOKEN_USER ** ppTokenUser
        )
    {
        DWORD dwRet = NO_ERROR;
        TOKEN_USER * ptokenUser = NULL;
    
        BOOL fRet = FALSE;
        DWORD dwSize = 0;

        _ASSERT(hToken != NULL);
        _ASSERT(ppTokenUser != NULL);

        if ( hToken == NULL ||
             ppTokenUser == NULL )
        {
            return ERROR_INVALID_PARAMETER;
        }

        *ppTokenUser = NULL;
    
        fRet = GetTokenInformation(hToken,
                                   TokenUser,
                                   NULL,
                                   0,
                                   &dwSize);
        if (FALSE == fRet)
        {
            if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
            {
                dwRet = GetLastError();
                goto exit;
            }
        }

        ptokenUser = (TOKEN_USER*)new BYTE[dwSize];
        if (NULL == ptokenUser)
        {
            dwRet = ERROR_NOT_ENOUGH_MEMORY;
            goto exit;
        }
    
        fRet = GetTokenInformation(hToken,
                                TokenUser,
                                ptokenUser,
                                dwSize,
                                &dwSize);
        if (FALSE == fRet)
        {
            dwRet = GetLastError();
            goto exit;
        }

        *ppTokenUser = ptokenUser;
        ptokenUser = NULL;
        dwRet = NO_ERROR;

    exit:

        // Free the local token owner variable if
        // it is still set.  In this case it is set
        // because there was an error, otherwise it
        // will have all ready been changed to NULL.
        if ( ptokenUser != NULL )
        {
            _ASSERT( dwRet != NO_ERROR );

            FreeTokenUserSID( &ptokenUser );
        }

        return dwRet;
    }

    static
    HRESULT
    CUtils::CreatePipeSecurity(
        __out PSECURITY_ATTRIBUTES *ppSa
    )
    /*++

    Routine Description:

        The CreatePipeSecurity function creates and initializes a new
        SECURITY_ATTRIBUTES structure to allow Local System full
        access to the pipe.

    Arguments:

        ppSa - output a pointer to a SECURITY_ATTRIBUTES structure that
               allows Local System full access to the pipe.
               The structure must be freed by calling FreePipeSecurity.

    Return Value:

        HRESULT

    --*/
    {
        HRESULT              hr                     = S_OK;
        DWORD                dwError                = ERROR_SUCCESS;

        PSECURITY_DESCRIPTOR pSd                    = NULL;
        PSECURITY_ATTRIBUTES pSa                    = NULL;

        LPWSTR               lpwszCurrentUserSid    = NULL;

        HANDLE               hProcessToken          = NULL;
        TOKEN_USER *         pTokenUser             = NULL;

        WCHAR                pszSDDL[256];

        //
        // Get Current User SID
        //

        if(! OpenProcessToken(GetCurrentProcess(), GENERIC_READ, &hProcessToken) )
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Finished;
        }

        //
        // Get Token for the current user.
        // Free the token user with the FreeTokenUserSid API when done.
        //

        dwError = GetTokenUserSID(hProcessToken, &pTokenUser);
        if ( dwError != ERROR_SUCCESS )
        {
            hr = HRESULT_FROM_WIN32(dwError);
            goto Finished;
        }

        //
        // Convert the SID to string
        //

        if (!ConvertSidToStringSidW(pTokenUser->User.Sid, &lpwszCurrentUserSid))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Finished;
        }

        //
        // Construct the SDDL :
        // Dacl : Protected
        // GA (Generic Access) for Local System,
        // GA for Current User Sid
        //
        wcscpy(pszSDDL, L"D:P(A;OICI;GA;;;SY)(A;OICI;GA;;;");
        wcscat(pszSDDL, lpwszCurrentUserSid);
        wcscat(pszSDDL, L")");

        //
        // Create the Security Descriptor from string SDDL
        //

        if ( !ConvertStringSecurityDescriptorToSecurityDescriptorW(
                pszSDDL,
                SDDL_REVISION_1,
                &pSd,
                NULL) )
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Finished;
        }

        //
        // Allocate the memory of SECURITY_ATTRIBUTES.
        //

        pSa = (PSECURITY_ATTRIBUTES) LocalAlloc( LPTR, sizeof(*pSa) );
        if (pSa == NULL)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Finished;
        }

        pSa->nLength = sizeof(*pSa);
        pSa->lpSecurityDescriptor = pSd;
        pSa->bInheritHandle = FALSE;

        *ppSa = pSa;

        pSa = NULL;
        pSd = NULL;

    Finished:

        if ( lpwszCurrentUserSid )
        {
            LocalFree( lpwszCurrentUserSid );
            lpwszCurrentUserSid = NULL;
        }

        if ( hProcessToken )
        {
            CloseHandle(hProcessToken);
            hProcessToken = NULL;
        }

        if ( pTokenUser )
        {
            FreeTokenUserSID( &pTokenUser );
            pTokenUser = NULL;
        }

        if (pSd)
        {
            LocalFree(pSd);
            pSd = NULL;
        }

        if (pSa)
        {
            LocalFree(pSa);
            pSa = NULL;
        }

        return hr;
    }

    static
    void
    CUtils::FreePipeSecurity(
        __in PSECURITY_ATTRIBUTES pSa
    )
    /*++

    Routine Description:

        The FreePipeSecurity function frees a SECURITY_ATTRIBUTES
        structure that was created by the CreatePipeSecurity function.

    Arguments:

        pSa - pointer to a SECURITY_ATTRIBUTES structure that was created by
              the CreatePipeSecurity function.

    Return Value:

        VOID

    --*/
    {
        if (pSa)
        {
            if (pSa->lpSecurityDescriptor)
            {
                LocalFree(pSa->lpSecurityDescriptor);
            }

            LocalFree(pSa);
        }
    }

    static 
    HRESULT 
    GetElementDWORDProperty(
        IAppHostElement     *pElement,
        LPCWSTR              pszPropName,
        DWORD               *pdwPropValue
    )
    {
        HRESULT             hr = S_OK;
        BSTR                bstrPropName = SysAllocString( pszPropName );
        IAppHostProperty   *pProperty = NULL;
        VARIANT             varValue;

        VariantInit(&varValue);

        ErrorIf(bstrPropName == NULL, E_OUTOFMEMORY);
        CheckError(pElement->GetPropertyByName( bstrPropName, &pProperty ));
        CheckError(pProperty->get_Value( &varValue ));
        CheckError(VariantChangeType( &varValue, &varValue, 0, VT_UI4 ));

        *pdwPropValue = varValue.ulVal;

        hr = S_OK;
    Error:
        VariantClear( &varValue );

        if(pProperty)
        {
            pProperty->Release();
            pProperty = NULL;
        }

        if(bstrPropName)
        {
            SysFreeString(bstrPropName);
        }

        return hr;
    }

    static
    HRESULT
    GetAdminElement(
        IN      IAppHostAdminManager *      pAdminMgr,
        IN      LPCWSTR                     pszConfigPath,
        IN      LPCWSTR                     pszElementName,
        OUT     IAppHostElement **          ppElement
    )
    {
        HRESULT hr = S_OK;
        BSTR bstrConfigPath = NULL;
        BSTR bstrElementName = NULL;

        bstrConfigPath = SysAllocString(pszConfigPath);
        bstrElementName = SysAllocString(pszElementName);

        ErrorIf(bstrConfigPath == NULL, E_OUTOFMEMORY);
        ErrorIf(bstrElementName == NULL, E_OUTOFMEMORY);

        CheckError(pAdminMgr->GetAdminSection( bstrElementName,
                                         bstrConfigPath,
                                         ppElement ));

        hr = S_OK; //do cleanup
    Error:

        if ( bstrElementName != NULL )
        {
            SysFreeString(bstrElementName);
            bstrElementName = NULL;
        }
        if ( bstrConfigPath != NULL )
        {
            SysFreeString(bstrConfigPath);
            bstrConfigPath = NULL;
        }

        return hr;
    }

    static
    HRESULT
    GetElementStringProperty(
        IN          IAppHostElement    *pElement,
        IN          LPCWSTR             pszPropName,
        OUT         BSTR               *ppbstrPropValue
    )
    {
        HRESULT hr = S_OK;
        BSTR bstrPropName = SysAllocString( pszPropName );
        IAppHostProperty* pProperty = NULL;

        *ppbstrPropValue = NULL;

        ErrorIf(bstrPropName == NULL, E_OUTOFMEMORY);

        CheckError(pElement->GetPropertyByName( bstrPropName, &pProperty ));

        CheckError(pProperty->get_StringValue( ppbstrPropValue ));

        hr = S_OK; // do cleanup
    Error:
        if (pProperty)
        {
            pProperty->Release();
        }

        if (bstrPropName)
        {
            SysFreeString( bstrPropName );
        }

        return hr;
    }

    static
    HRESULT
    FindNextElement(
        IN      IAppHostElementCollection          *pCollection,
        IN OUT  ENUM_INDEX                         *pIndex,
        OUT     IAppHostElement                   **ppElement
    )
    {
        HRESULT hr;

        *ppElement = NULL;

        if (pIndex->Index.ulVal >= pIndex->Count)
        {
            return S_FALSE;
        }

        hr = pCollection->get_Item(pIndex->Index, ppElement);

        if (SUCCEEDED(hr))
        {
            pIndex->Index.ulVal++;
        }

        return hr;
    }

    static
    HRESULT
    FindFirstElement(
        IN      IAppHostElementCollection          *pCollection,
        OUT     ENUM_INDEX                         *pIndex,
        OUT     IAppHostElement                   **ppElement
    )
    {
        HRESULT hr;

        CheckError(pCollection->get_Count(&pIndex->Count));

        //TODO: check if VariantClear is needed in this case.
        VariantInit(&pIndex->Index);
        pIndex->Index.vt = VT_UI4;
        pIndex->Index.ulVal = 0;

        return FindNextElement(pCollection, pIndex, ppElement);
    Error:
        return hr;
    }

private:
    CUtils()
    {
    }
};

#endif