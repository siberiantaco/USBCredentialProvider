//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "Unit4.h"
#include "registry.hpp"
#include "windows.h"
#include "FileCtrl.hpp";
#include "imagehlp.h"
TRegistry *reg, *myReg;

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm4 *Form4;
//---------------------------------------------------------------------------
__fastcall TForm4::TForm4(TComponent* Owner)
	: TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TForm4::FormCreate(TObject *Sender)
{
	reg = new TRegistry;
	myReg = new TRegistry;
	TStringList *list = new TStringList;
	TStringList *myList = new TStringList;

	reg->RootKey = HKEY_LOCAL_MACHINE;
	reg->OpenKeyReadOnly("SECURITY\\SAM\\Domains\\Account\\Users\\Names");
	reg->GetKeyNames(list);

	myReg->RootKey = HKEY_LOCAL_MACHINE;
	if (!myReg->KeyExists("SOFTWARE\\USBCredProv\\Users"))
		myReg->CreateKey("SOFTWARE\\USBCredProv\\Users");
	myReg->OpenKey("SOFTWARE\\USBCredProv\\Users", false);
	myReg->GetValueNames(myList);

	for (int i = 0; i < list->Count; i++)
	{
		for (int j = 0; j < myList->Count; j++)
		{
			if (list->Strings[i] == myList->Strings[j])
				list->Delete(i);
		}
	}
	ListBox1->Items = list;
	ListBox2->Items = myList;
	delete list;
	delete myList;
}
//---------------------------------------------------------------------------

void __fastcall TForm4::Button1Click(TObject *Sender)
{
	UnicodeString key = L"";
	key = InputBox(L"", L"Ключ", L"");
	if (key != L"")
	{
		UnicodeString dir = L"";
		SelectDirectory(L"Выберите носитель", L"", dir);
		if (dir != L"")
		{
			dir += "\\cred.key";
			HANDLE hFile = CreateFile(dir.c_str(), GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, NULL, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				DWORD dummy = 0;
				if (WriteFile(hFile, key.c_str(), key.Length() * sizeof(wchar_t), &dummy, NULL))
				{
					DWORD checkSum = 0;
					CheckSumMappedFile(key.c_str(), key.Length() * sizeof(wchar_t), &dummy, &checkSum);
					int index = ListBox1->ItemIndex;
					UnicodeString name = ListBox1->Items->Strings[index];
					ListBox1->Items->Delete(index);
					ListBox2->Items->Add(name);
					myReg->WriteInteger(name, (int)checkSum);
				}
				else
				{
					MessageBox(this->Handle, L"Ошибка при записи в файл.", L"", MB_ICONERROR);
					return;
				}
				CloseHandle(hFile);
			}
			else
			{
				MessageBox(this->Handle, L"Ошибка при создании файла.", L"", MB_ICONERROR);
				return;
			}
		}
	}
}
//---------------------------------------------------------------------------

void __fastcall TForm4::Button2Click(TObject *Sender)
{
	int index = ListBox2->ItemIndex;
	UnicodeString name = ListBox2->Items->Strings[index];
	ListBox2->Items->Delete(index);
	ListBox1->Items->Add(name);
	myReg->DeleteValue(name);
}
//---------------------------------------------------------------------------


