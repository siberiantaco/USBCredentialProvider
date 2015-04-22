
#define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif

#include "CSampleCredential.h"
#include "guid.h"
#include "imagehlp.h"
#pragma comment(lib, "Lib_ImageHlp")
#include <sstream>


// CSampleCredential ////////////////////////////////////////////////////////

// NOTE: Please read the readme.txt file to understand when it's appropriate to
// wrap an another credential provider and when it's not.  If you have questions
// about whether your scenario is an appropriate use of wrapping another credprov,
// please contact credprov@microsoft.com
CSampleCredential::CSampleCredential():
    _cRef(1),
    _pCredProvCredentialEvents(NULL)
{
    DllAddRef();
    ZeroMemory(_rgCredProvFieldDescriptors, sizeof(_rgCredProvFieldDescriptors));
    ZeroMemory(_rgFieldStatePairs, sizeof(_rgFieldStatePairs));
    ZeroMemory(_rgFieldStrings, sizeof(_rgFieldStrings));
    _pWrappedCredential = NULL;
    _dwWrappedDescriptorCount = 0;
    _dwDatabaseIndex = 0;

}

CSampleCredential::~CSampleCredential()
{
    for (int i = 0; i < ARRAYSIZE(_rgFieldStrings); i++)
    {
        CoTaskMemFree(_rgFieldStrings[i]);
        CoTaskMemFree(_rgCredProvFieldDescriptors[i].pszLabel);
    }

    DllRelease();
}

// Initializes one credential with the field information passed in. We also keep track
// of our wrapped credential and how many fields it has.
HRESULT CSampleCredential::Initialize(
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* rgcpfd,
    const FIELD_STATE_PAIR* rgfsp,
    ICredentialProviderCredential *pWrappedCredential,
    DWORD dwWrappedDescriptorCount
    )
{
    HRESULT hr = S_OK;

    // Grab the credential we're wrapping for future reference.
    if (_pWrappedCredential != NULL)
    {
        _pWrappedCredential->Release();
    }
    _pWrappedCredential = pWrappedCredential;
    _pWrappedCredential->AddRef();

    // We also need to remember how many fields the inner credential has.
    _dwWrappedDescriptorCount = dwWrappedDescriptorCount;

    // Copy the field descriptors for each field. This is useful if you want to vary the field
    // descriptors based on what Usage scenario the credential was created for.
    for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(_rgCredProvFieldDescriptors); i++)
    {
        _rgFieldStatePairs[i] = rgfsp[i];
        hr = FieldDescriptorCopy(rgcpfd[i], &_rgCredProvFieldDescriptors[i]);
    }

    // Initialize the String value of all of our fields.
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"I Work In:", &_rgFieldStrings[SFI_I_WORK_IN_STATIC]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"Database", &_rgFieldStrings[SFI_DATABASE_COMBOBOX]);
    }

    return hr;
}

// LogonUI calls this in order to give us a callback in case we need to notify it of 
// anything. We'll also provide it to the wrapped credential.
HRESULT CSampleCredential::Advise(
    ICredentialProviderCredentialEvents* pcpce
    )
{
    HRESULT hr = S_OK;

    // Grab one for ourself.
    if (_pCredProvCredentialEvents != NULL)
    {
        _pCredProvCredentialEvents->Release();
    }
    _pCredProvCredentialEvents = pcpce;
    _pCredProvCredentialEvents->AddRef();

    // Pass it along for the wrapped credential to use.
    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->Advise(pcpce);
    }

    return hr;
}

// LogonUI calls this to tell us to release the callback. 
// We'll also provide it to the wrapped credential.
HRESULT CSampleCredential::UnAdvise()
{
    HRESULT hr = S_OK;

    // Release our reference.
    if (_pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->Release();
    }
    _pCredProvCredentialEvents = NULL;

    // Allow the wrapped credential to release its reference.
    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->UnAdvise();
    }

    return hr;
}

// LogonUI calls this function when our tile is selected (zoomed)
// If you simply want fields to show/hide based on the selected state,
// there's no need to do anything here - you can set that up in the 
// field definitions. In fact, we're just going to hand it off to the
// wrapped credential in case it wants to do something.
HRESULT CSampleCredential::SetSelected(BOOL* pbAutoLogon)  
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetSelected(pbAutoLogon);
    }

    return hr;
}

// Similarly to SetSelected, LogonUI calls this when your tile was selected
// and now no longer is. We'll let the wrapped credential do anything it needs.
HRESULT CSampleCredential::SetDeselected()
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetDeselected();
    }

    return hr;
}

// Get info for a particular field of a tile. Called by logonUI to get information to 
// display the tile. We'll check to see if it's for us or the wrapped credential, and then
// handle or route it as appropriate.
HRESULT CSampleCredential::GetFieldState(
    DWORD dwFieldID,
    CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis
    )
{
    HRESULT hr = E_UNEXPECTED;

    // Make sure we have a wrapped credential.
    if (_pWrappedCredential != NULL)
    {
        // Validate parameters.
        if ((pcpfs != NULL) && (pcpfis != NULL))
        {
            // If the field is in the wrapped credential, hand it off.
            if (_IsFieldInWrappedCredential(dwFieldID))
            {
                hr = _pWrappedCredential->GetFieldState(dwFieldID, pcpfs, pcpfis);
            }
            // Otherwise, we need to see if it's one of ours.
            else
            {
                FIELD_STATE_PAIR *pfsp = _LookupLocalFieldStatePair(dwFieldID);
                // If the field ID is valid, give it info it needs.
                if (pfsp != NULL)
                {
                    *pcpfs = pfsp->cpfs;
                    *pcpfis = pfsp->cpfis;

                    hr = S_OK;
                }
                else
                {
                    hr = E_INVALIDARG;
                }
            }
        }
        else
        {
            hr = E_INVALIDARG;
        }
    }
    return hr;
}

// Sets ppwsz to the string value of the field at the index dwFieldID. We'll check to see if 
// it's for us or the wrapped credential, and then handle or route it as appropriate.
HRESULT CSampleCredential::GetStringValue(
    DWORD dwFieldID, 
    PWSTR* ppwsz
    )
{
    HRESULT hr = E_UNEXPECTED;

    // Make sure we have a wrapped credential.
    if (_pWrappedCredential != NULL)
    {
        // If this field belongs to the wrapped credential, hand it off.
        if (_IsFieldInWrappedCredential(dwFieldID))
        {
            hr = _pWrappedCredential->GetStringValue(dwFieldID, ppwsz);
        }
        // Otherwise determine if we need to handle it.
        else
        {
            FIELD_STATE_PAIR *pfsp = _LookupLocalFieldStatePair(dwFieldID);
            if (pfsp != NULL)
            {
                hr = SHStrDupW(_rgFieldStrings[SFI_I_WORK_IN_STATIC], ppwsz);
            }
            else
            {
                hr = E_INVALIDARG;
            }
        }
    }
    return hr;
}

// Returns the number of items to be included in the combobox (pcItems), as well as the 
// currently selected item (pdwSelectedItem). We'll check to see if it's for us or the 
// wrapped credential, and then handle or route it as appropriate.
HRESULT CSampleCredential::GetComboBoxValueCount(
    DWORD dwFieldID, 
    DWORD* pcItems, 
    DWORD* pdwSelectedItem
    )
{
    HRESULT hr = E_UNEXPECTED;

    // Make sure we have a wrapped credential.
    if (_pWrappedCredential != NULL)
    {
        // If this field belongs to the wrapped credential, hand it off.
        if (_IsFieldInWrappedCredential(dwFieldID))
        {
            hr = _pWrappedCredential->GetComboBoxValueCount(dwFieldID, pcItems, pdwSelectedItem);
        }
        // Otherwise determine if we need to handle it.
        else
        {
            FIELD_STATE_PAIR *pfsp = _LookupLocalFieldStatePair(dwFieldID);
            if (pfsp != NULL)
            {
                *pcItems = ARRAYSIZE(s_rgDatabases);
                *pdwSelectedItem = _dwDatabaseIndex;
                hr = S_OK;
            }
            else
            {
                hr = E_INVALIDARG;
            }
        }
    }

    return hr;
}

// Called iteratively to fill the combobox with the string (ppwszItem) at index dwItem.
// We'll check to see if it's for us or the wrapped credential, and then handle or route 
// it as appropriate.
HRESULT CSampleCredential::GetComboBoxValueAt(
    DWORD dwFieldID, 
    DWORD dwItem,
    PWSTR* ppwszItem
    )
{
    HRESULT hr = E_UNEXPECTED;

    // Make sure we have a wrapped credential.
    if (_pWrappedCredential != NULL)
    {
        // If this field belongs to the wrapped credential, hand it off.
        if (_IsFieldInWrappedCredential(dwFieldID))
        {
            hr = _pWrappedCredential->GetComboBoxValueAt(dwFieldID, dwItem, ppwszItem);
        }
        // Otherwise determine if we need to handle it.
        else
        {
            FIELD_STATE_PAIR *pfsp = _LookupLocalFieldStatePair(dwFieldID);
            if ((pfsp != NULL) && (dwItem < ARRAYSIZE(s_rgDatabases)))
            {
                hr = SHStrDupW(s_rgDatabases[dwItem], ppwszItem);
            }
            else
            {
                hr = E_INVALIDARG;
            }
        }
    }

    return hr;
}

// Called when the user changes the selected item in the combobox. We'll check to see if 
// it's for us or the wrapped credential, and then handle or route it as appropriate.
HRESULT CSampleCredential::SetComboBoxSelectedValue(
    DWORD dwFieldID,
    DWORD dwSelectedItem
    )
{
    HRESULT hr = E_UNEXPECTED;

    // Make sure we have a wrapped credential.
    if (_pWrappedCredential != NULL)
    {
        // If this field belongs to the wrapped credential, hand it off.
        if (_IsFieldInWrappedCredential(dwFieldID))
        {
            hr = _pWrappedCredential->SetComboBoxSelectedValue(dwFieldID, dwSelectedItem);
        }
        // Otherwise determine if we need to handle it.
        else
        {
            FIELD_STATE_PAIR *pfsp = _LookupLocalFieldStatePair(dwFieldID);
            if ((pfsp != NULL) && (dwSelectedItem < ARRAYSIZE(s_rgDatabases)))
            {
                _dwDatabaseIndex = dwSelectedItem;
                hr = S_OK;
            }
            else
            {
                hr = E_INVALIDARG;
            }
        }
    }

    return hr;
}

//------------- 
// The following methods are for logonUI to get the values of various UI elements and 
// then communicate to the credential about what the user did in that field. Even though
// we don't offer these field types ourselves, we need to pass along the request to the
// wrapped credential.

HRESULT CSampleCredential::GetBitmapValue(
    DWORD dwFieldID, 
    HBITMAP* phbmp
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->GetBitmapValue(dwFieldID, phbmp);
    }

    return hr;
}

HRESULT CSampleCredential::GetSubmitButtonValue(
    DWORD dwFieldID,
    DWORD* pdwAdjacentTo
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->GetSubmitButtonValue(dwFieldID, pdwAdjacentTo);
    }

    return hr;
}

HRESULT CSampleCredential::SetStringValue(
    DWORD dwFieldID,
    PCWSTR pwz
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetStringValue(dwFieldID, pwz);
    }

    return hr;

}

HRESULT CSampleCredential::GetCheckboxValue(
    DWORD dwFieldID, 
    BOOL* pbChecked,
    PWSTR* ppwszLabel
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        if (_IsFieldInWrappedCredential(dwFieldID))
        {
            hr = _pWrappedCredential->GetCheckboxValue(dwFieldID, pbChecked, ppwszLabel);
        }
    }

    return hr;
}

HRESULT CSampleCredential::SetCheckboxValue(
    DWORD dwFieldID, 
    BOOL bChecked
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->SetCheckboxValue(dwFieldID, bChecked);
    }

    return hr;
}

HRESULT CSampleCredential::CommandLinkClicked(DWORD dwFieldID)
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->CommandLinkClicked(dwFieldID);
    }

    return hr;
}
//------ end of methods for controls we don't have ourselves ----//


DWORD GetKey(void ** key, DWORD * keySize)
{
	char fileName[] = ":\\cred.key";
	char volName[MAX_PATH] = "";
	char volPathName[(MAX_PATH + 1) * sizeof(char)] = "";
	DWORD retSize = NULL;
	char * dir = NULL;
	HANDLE hFile = NULL;
	HANDLE hList = FindFirstVolume(volName, MAX_PATH);
	if (hList == INVALID_HANDLE_VALUE)
		return ERROR;
	do
	{
		if (GetVolumePathNamesForVolumeName(volName, volPathName, (MAX_PATH + 1) * sizeof(char), &retSize))
		{
			dir = new char[sizeof fileName + 1];
			strncpy(dir, volPathName, 1);
			strncpy((dir + 1), fileName, sizeof fileName);
			hFile = CreateFile(dir, GENERIC_READ, NULL, NULL, OPEN_EXISTING, NULL, NULL);			
			delete[] dir;
			if (hFile != INVALID_HANDLE_VALUE)
			{
				*keySize = 0;
				*keySize = GetFileSize(hFile, keySize);

				*key = new BYTE[*keySize];
				
				ReadFile(hFile, *key, *keySize, keySize, NULL);	
				
				CloseHandle(hFile);
				return 1;
			}
		}
	} while (FindNextVolume(hList, volName, MAX_PATH));
	return ERROR_NOT_FOUND;
}

//
// Collect the username and password into a serialized credential for the correct usage scenario 
// (checking key, logon/unlock is what's demonstrated in this sample).  LogonUI then passes these credentials 
// back to the system to log on.
//
HRESULT CSampleCredential::GetSerialization(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs, 
    PWSTR* ppwszOptionalStatusText, 
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon
    )
{
    HRESULT hr = E_UNEXPECTED;
	DWORD keySize = 0;
	void * key = 0;
	DWORD res = GetKey(&key, &keySize);
	DWORD dummy = 0, checkSum = 0;
	HKEY hRegKey = 0;
	if (res == ERROR)
	{
		MessageBox(NULL, TEXT("Unexpected error."), TEXT("Error"), 0);
		return hr;
	}	
	else if(res == ERROR_NOT_FOUND)
	{
		MessageBox(NULL, TEXT("Cannot find the specified file."), TEXT("Error"), 0);
		return hr;
	}
	CheckSumMappedFile(key, keySize, &dummy, &checkSum);
	if (RegOpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\USBCredProv\\Users", &hRegKey) != ERROR_SUCCESS)
	{
		MessageBox(NULL, TEXT("Cannot open registry key."), TEXT("Error"), 0);
		return hr;
	}

	PWSTR userName = 0;
	_pWrappedCredential->GetStringValue(0, &userName);
	DWORD size = sizeof DWORD;
	BYTE * regCheckSumArr = new BYTE[size];
	if ((RegQueryValueExW(hRegKey, userName, NULL, NULL, regCheckSumArr, &size)) != ERROR_SUCCESS)
	{
		MessageBox(NULL, TEXT("Cannot get key value."), TEXT("Error"), 0);
		return hr;
	}
	DWORD regCheckSum = (DWORD)((regCheckSumArr[0]) | (regCheckSumArr[1] << 8) | (regCheckSumArr[2] << 16) | (regCheckSumArr[3] << 24));

	if (regCheckSum == checkSum)
	{
		if (_pWrappedCredential != NULL)
		{
			hr = _pWrappedCredential->GetSerialization(pcpgsr, pcpcs, ppwszOptionalStatusText, pcpsiOptionalStatusIcon);
		}		
	}
	else
	{
		MessageBox(NULL, TEXT("Incorrect key."), TEXT("Error"), 0);
		return hr;
	}
	return hr;
		
}

// ReportResult is completely optional. However, we will hand it off to the wrapped
// credential in case they want to handle it.
HRESULT CSampleCredential::ReportResult(
    NTSTATUS ntsStatus, 
    NTSTATUS ntsSubstatus,
    PWSTR* ppwszOptionalStatusText, 
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon
    )
{
    HRESULT hr = E_UNEXPECTED;

    if (_pWrappedCredential != NULL)
    {
        hr = _pWrappedCredential->ReportResult(ntsStatus, ntsSubstatus, ppwszOptionalStatusText, pcpsiOptionalStatusIcon);
    }

    return hr;
}

BOOL CSampleCredential::_IsFieldInWrappedCredential(
    DWORD dwFieldID
    )
{
    return (dwFieldID < _dwWrappedDescriptorCount);
}

FIELD_STATE_PAIR *CSampleCredential::_LookupLocalFieldStatePair(
    DWORD dwFieldID
    )
{
    // Offset into the ID to account for the wrapped fields.
    dwFieldID -= _dwWrappedDescriptorCount;

    // If the index if valid, give it the info it wants.
    if (dwFieldID < SFI_NUM_FIELDS)
    {
        return &(_rgFieldStatePairs[dwFieldID]);
    }
    
    return NULL;
}