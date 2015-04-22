This is solution for term paper "Two factor authentication for Windows 7" for Operating Systems Secutiry course. Project consists of one GUI application (Credential manager) and one DLL (USBCredentialProvider).

____________________________________

- USBCredentialProvider

This is VS2010 project. Credential provider is DDL which uses two COM-interfaces: ICredentialProvider and ICredentialProviderCredential. 

More about COM: 
	en.wikipedia.org/wiki/Component_Object_Model

More about ICredentialProvider and ICredentialProviderCredential interfaces: 
	msdn.microsoft.com/en-us/library/windows/desktop/bb776042(v=vs.85).aspx 
	msdn.microsoft.com/en-us/library/windows/desktop/bb776029(v=vs.85).aspx

The main idea is to find key-file in any storage device and compare its digest with digest saved in Windows Registry. Аll these steps are implemented by CSampleCredential::GetSerialization and GetKey methods in CSampleCredential.cpp.

This DLL wraps default Credential Provider so the rest of code except methods mentioned above is about it. This code is based on examples from MSDN.
____________________________________

- Credential manager

This one is GUI which implements convinient way to create key-files for any user and add digest values to Windows Registry. C++ Builder XE5 Project.
____________________________________
