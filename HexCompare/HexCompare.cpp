

#include "stdafx.h"
#include "commctrl.h"
#include "strsafe.h"
#include "HexCompare.h"

#define MAX_LOADSTRING 100

#define BUFFER MAX_PATH 

enum myID {
	ID_TEXTBOX_1,
	ID_TEXTBOX_2,
	ID_LISTBOX_1,
	ID_LISTBOX_2,
	ID_BUTTON_LOAD_1,
	ID_BUTTON_LOAD_2,
	ID_BUTTON_COMPARE,
	ID_BUTTON_CANCEL,
	ID_BUTTON_NEXT,
	ID_BUTTON_PREV,
	ID_LABEL_1,
	ID_LABEL_2,
	ID_PROGRESSBAR
};

// Глобальные переменные:
HINSTANCE hInst;                                
WCHAR szTitle[MAX_LOADSTRING];                 
WCHAR szWindowClass[MAX_LOADSTRING];

HWND hList1;
HWND hList2;

DWORD cbView;
unsigned long long MAXcbFile;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL				initInterface(HWND hWnd);
VOID				getFilePath(HWND hWnd, HWND hEdit);
DWORD WINAPI		MyThreadFunction(LPVOID lpParam);
void				AddItem(HWND hwnd, LPCWSTR pstr, unsigned char data);
void				loadPage(const WCHAR *pszFileName1, const WCHAR *pszFileName2, unsigned long long offset);
LPWSTR				toHex(const unsigned char *pView, int i, int offset);
BOOL				compare(const WCHAR *pszFileName1, const WCHAR *pszFileName2, HWND hPrBar);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_HEXCOMPARE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HEXCOMPARE));

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HEXCOMPARE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_HEXCOMPARE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

   HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, CW_USEDEFAULT, 0, 800, 730, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HANDLE hThread;
	static DWORD id;
	static unsigned long long offset;
	static HWND hEdit1;
	static HWND hEdit2;
	static TCHAR buff1[MAX_LOADSTRING];
	static TCHAR buff2[MAX_LOADSTRING];

	switch (message)
	{
	case WM_CREATE:
	{
		if (!initInterface(hWnd))
			return EXIT_FAILURE;

		offset = 0;
		hEdit1 = GetDlgItem(hWnd, ID_TEXTBOX_1);
		hEdit2 = GetDlgItem(hWnd, ID_TEXTBOX_2);
		SYSTEM_INFO sysinfo{};
		GetSystemInfo(&sysinfo);
		cbView = sysinfo.dwAllocationGranularity;
	}
	break;
	case WM_DRAWITEM:
	{
		PDRAWITEMSTRUCT pdis = (PDRAWITEMSTRUCT)lParam;

		// If there are no list box items, skip this message. 
		if (pdis->itemID == -1)
		{
			break;
		}

		switch (pdis->itemAction)
		{
		case ODA_SELECT:
		case ODA_DRAWENTIRE:
		{
			TCHAR achBuffer[BUFFER];
			size_t cch;
			unsigned char data; //информация о совпадении 8 байт (по 1 биту на байт)
			int yPos;
			TEXTMETRIC tm;
			SIZE sz;

			SendMessage(pdis->hwndItem, LB_GETTEXT,	pdis->itemID, (LPARAM)achBuffer);

			data = (unsigned char)SendMessage(pdis->hwndItem, LB_GETITEMDATA, pdis->itemID, NULL);

			SetTextColor(pdis->hDC, RGB(0, 0, 0));
			if (pdis->itemState & ODS_SELECTED)
			{
				SetBkColor(pdis->hDC, RGB(128, 128, 128));
				FillRect(pdis->hDC, &pdis->rcItem, (HBRUSH)GetStockObject(GRAY_BRUSH));
			}
			else
			{
				SetBkColor(pdis->hDC, RGB(255, 255, 255));
				FillRect(pdis->hDC, &pdis->rcItem, (HBRUSH)GetStockObject(WHITE_BRUSH));
			}

			GetTextMetrics(pdis->hDC, &tm);
			yPos = (pdis->rcItem.bottom + pdis->rcItem.top - tm.tmHeight) / 2;

			StringCchLength(achBuffer, BUFFER, &cch);

			int offset = 0;
			int i = 0;
			int sizeAdr = 10;

			for (; i < sizeAdr; i++)//вывод адреса
			{
				TextOut(pdis->hDC, pdis->rcItem.left + offset, yPos, &achBuffer[i], 1);
				GetTextExtentPoint32(pdis->hDC, &achBuffer[i], 1, &sz);
				offset += sz.cx;
			}

			offset = 9 * sizeAdr;
			TextOut(pdis->hDC, pdis->rcItem.left + offset, yPos, L":", 1);
			offset += 175;
			TextOut(pdis->hDC, pdis->rcItem.left + offset, yPos, L"|", 1);

			int offsetASCII = offset + 15;

			for (int i = 0; i < 8; i++)
			{
				//выделение несовпадающих байт
				if (data & (1 << i))
				{
					SetTextColor(pdis->hDC, RGB(0, 0, 0));
				}
				else
				{
					SetTextColor(pdis->hDC, RGB(255, 0, 0));
				}

				int index;
				offset = 10 * sizeAdr + i * 20;
				for (int j = 0; j < 2; j++) //вывод байт
				{
					index = sizeAdr + i * 2 + j;
					TextOut(pdis->hDC, pdis->rcItem.left + offset, yPos, &achBuffer[index], 1);
					GetTextExtentPoint32(pdis->hDC, &achBuffer[index], 1, &sz);
					offset += sz.cx;
				}

				//вывод ASCII
				index = i + sizeAdr + 16;
				TextOut(pdis->hDC, pdis->rcItem.left + offsetASCII, yPos, &achBuffer[index], 1);
				GetTextExtentPoint32(pdis->hDC, &achBuffer[index], 1, &sz);
				offsetASCII += sz.cx;
			}
		}
			break;
		case ODA_FOCUS:
			break;
		}
		return TRUE;
	}
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
			case ID_BUTTON_LOAD_1:
			{
				SendMessage(hList1, LB_RESETCONTENT, 0, 0);
				SendMessage(hList2, LB_RESETCONTENT, 0, 0);
				getFilePath(hWnd, hEdit1);
			}
				break;

			case ID_BUTTON_LOAD_2:
			{
				SendMessage(hList1, LB_RESETCONTENT, 0, 0);
				SendMessage(hList2, LB_RESETCONTENT, 0, 0);
				getFilePath(hWnd, hEdit2);
			}
				break;
			case ID_BUTTON_COMPARE:
			{
				SendMessage(hEdit1, WM_GETTEXT, MAX_LOADSTRING, (LPARAM)buff1);
				SendMessage(hEdit2, WM_GETTEXT, MAX_LOADSTRING, (LPARAM)buff2);

				SendMessage(hList1, LB_RESETCONTENT, 0, 0);
				SendMessage(hList2, LB_RESETCONTENT, 0, 0);
				offset = 0;
				loadPage(buff1, buff2, offset);
 
				hThread = CreateThread(NULL, 0,	MyThreadFunction, hWnd,	0,	&id);
			}
				break;
			case ID_BUTTON_NEXT:
			{
				if (offset + cbView < MAXcbFile)
				{
					offset += cbView;
					loadPage(buff1, buff2, offset);
				}
			}
			break;			
			case ID_BUTTON_PREV:
			{
				if (offset != 0)
				{
					offset -= cbView;
					loadPage(buff1, buff2, offset);
				}
			}
			break;
			case ID_BUTTON_CANCEL:
			{
				TerminateThread(hThread, EXIT_SUCCESS);
			}
				break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

BOOL initInterface(HWND hWnd)
{
	RECT clientArea;
	GetClientRect(hWnd, &clientArea);

	const int DEFAULT_HEIGHT = 20;

	const SIZE spaceBtwContrls{ 5,5 };

	SIZE sizeList{ (clientArea.right - 3 * spaceBtwContrls.cx) / 2,clientArea.bottom - DEFAULT_HEIGHT * 4 - spaceBtwContrls.cy * 3};
	SIZE sizeButtonLoad{ 30,DEFAULT_HEIGHT };
	SIZE sizeEdit{ sizeList.cx - spaceBtwContrls.cx - sizeButtonLoad.cx, DEFAULT_HEIGHT };
	SIZE sizeLabel{sizeList.cx, DEFAULT_HEIGHT };
	SIZE sizeButtonCancel{ 20, DEFAULT_HEIGHT };
	SIZE sizeButtonCompare{100, DEFAULT_HEIGHT };
	SIZE sizeButtonPage{ sizeButtonCompare.cx, DEFAULT_HEIGHT };
	SIZE sizeProgressBar{ sizeLabel.cx * 2 - (sizeButtonCancel.cx + sizeButtonCompare.cx + spaceBtwContrls.cx), DEFAULT_HEIGHT };

	POINT posLabel1{spaceBtwContrls.cx, 0};
	POINT posLabel2{posLabel1.x + sizeLabel.cx + spaceBtwContrls.cx, 0};

	POINT posEdit1{ posLabel1.x, posLabel1.y + sizeLabel.cy };
	POINT posEdit2{ posLabel2.x, posLabel2.y + sizeLabel.cy };

	POINT posButtonLoad1{ posEdit1.x + sizeEdit.cx + spaceBtwContrls.cx, posEdit1.y};
	POINT posButtonLoad2{ posEdit2.x + sizeEdit.cx + spaceBtwContrls.cx, posEdit2.y };
	POINT posButtonCompare{posEdit1.x, posEdit1.y + sizeEdit.cy + spaceBtwContrls.cy};
	POINT posProgressBar{ posButtonCompare.x + sizeButtonCompare.cx + spaceBtwContrls.cx, posButtonCompare.y };
	POINT posButtonCancel{posProgressBar.x + sizeProgressBar.cx + spaceBtwContrls.cx, posButtonCompare.y};

	POINT posList1{ posEdit1.x,posButtonCompare.y + sizeButtonCompare.cy + spaceBtwContrls.cy };
	POINT posList2{ posEdit2.x,posButtonCompare.y + sizeButtonCompare.cy + spaceBtwContrls.cy };

	POINT posButtonPrev{ posList1.x, posList1.y + sizeList.cy};
	POINT posButtonNext{ posList2.x + sizeList.cx - sizeButtonPage.cx, posList2.y + sizeList.cy};

	HWND label1 = CreateWindowEx(0, L"STATIC", L"Путь №1:", WS_CHILD | WS_VISIBLE, posLabel1.x, posLabel1.y, sizeLabel.cx, sizeLabel.cy, hWnd, reinterpret_cast<HMENU>(ID_LABEL_1), 0, 0);
	if (label1 == INVALID_HANDLE_VALUE) return FALSE;

	HWND label2 = CreateWindowEx(0, L"STATIC", L"Путь №2:", WS_CHILD | WS_VISIBLE, posLabel2.x, posLabel2.y, sizeLabel.cx, sizeLabel.cy, hWnd, reinterpret_cast<HMENU>(ID_LABEL_2), 0, 0);
	if (label2 == INVALID_HANDLE_VALUE) return FALSE;

	HWND hEdit1 = CreateWindow(L"Edit", NULL, WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER, posEdit1.x, posEdit1.y, sizeEdit.cx, sizeEdit.cy, hWnd, reinterpret_cast<HMENU>(ID_TEXTBOX_1), hInst, 0);
	if (hEdit1 == INVALID_HANDLE_VALUE) return FALSE;

	HWND hEdit2 = CreateWindow(L"Edit", NULL, WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER, posEdit2.x, posEdit2.y, sizeEdit.cx, sizeEdit.cy, hWnd, reinterpret_cast<HMENU>(ID_TEXTBOX_2), hInst, 0);
	if (hEdit2 == INVALID_HANDLE_VALUE) return FALSE;

	hList1 = CreateWindow(L"listbox", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | LBS_OWNERDRAWFIXED, posList1.x, posList2.y, sizeList.cx, sizeList.cy, hWnd, reinterpret_cast<HMENU>(ID_LISTBOX_1), hInst, 0);
	if (hList1 == INVALID_HANDLE_VALUE) return FALSE;

	hList2 = CreateWindow(L"listbox", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | LBS_OWNERDRAWFIXED, posList2.x, posList2.y, sizeList.cx, sizeList.cy, hWnd, reinterpret_cast<HMENU>(ID_LISTBOX_2), hInst, 0);
	if (hList2 == INVALID_HANDLE_VALUE) return FALSE;

	HWND hwndButtonCompare = CreateWindow(L"BUTTON", L"Сравнить", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, posButtonCompare.x, posButtonCompare.y, sizeButtonCompare.cx, sizeButtonCompare.cy, hWnd, reinterpret_cast<HMENU>(ID_BUTTON_COMPARE), hInst, NULL);
	if (hwndButtonCompare == INVALID_HANDLE_VALUE) return FALSE;

	HWND hwndPB = CreateWindowEx(0, PROGRESS_CLASS, (LPTSTR)NULL, WS_CHILD | WS_VISIBLE, posProgressBar.x, posProgressBar.y, sizeProgressBar.cx, sizeProgressBar.cy, hWnd, reinterpret_cast<HMENU>(ID_PROGRESSBAR), hInst, NULL);
	if (hwndPB == INVALID_HANDLE_VALUE) return FALSE;

	HWND hwndButtonCancel = CreateWindow(L"BUTTON", L"X", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, posButtonCancel.x, posButtonCancel.y, sizeButtonCancel.cx, sizeButtonCancel.cy, hWnd, reinterpret_cast<HMENU>(ID_BUTTON_CANCEL), hInst, NULL);
	if (hwndButtonCancel == INVALID_HANDLE_VALUE) return FALSE;

	HWND hwndButton1 = CreateWindow(L"BUTTON", L">>", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, posButtonLoad1.x, posButtonLoad1.y, sizeButtonLoad.cx, sizeButtonLoad.cy, hWnd, reinterpret_cast<HMENU>(ID_BUTTON_LOAD_1), hInst, NULL);
	if (hwndButton1 == INVALID_HANDLE_VALUE) return FALSE;

	HWND hwndButton2 = CreateWindow(L"BUTTON", L">>", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, posButtonLoad2.x, posButtonLoad2.y, sizeButtonLoad.cx, sizeButtonLoad.cy, hWnd, reinterpret_cast<HMENU>(ID_BUTTON_LOAD_2), hInst, NULL);
	if (hwndButton2 == INVALID_HANDLE_VALUE) return FALSE;

	HWND hwndButtonNext = CreateWindow(L"BUTTON", L"NextPage", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, posButtonNext.x, posButtonNext.y, sizeButtonPage.cx, sizeButtonPage.cy, hWnd, reinterpret_cast<HMENU>(ID_BUTTON_NEXT), hInst, NULL);
	if (hwndButtonNext == INVALID_HANDLE_VALUE) return FALSE;

	HWND hwndButtonPrev = CreateWindow(L"BUTTON", L"PrevPage", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, posButtonPrev.x, posButtonPrev.y, sizeButtonPage.cx, sizeButtonPage.cy, hWnd, reinterpret_cast<HMENU>(ID_BUTTON_PREV), hInst, NULL);
	if (hwndButtonPrev == INVALID_HANDLE_VALUE) return FALSE;

	return TRUE;
}

VOID getFilePath(HWND hWnd, HWND hEdit)
{
	OPENFILENAME ofn;     
	TCHAR szFile[260] = { 0 };

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = L"All\0*.*\0Text\0*.TXT\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn) == TRUE)
	{
		SendMessage(hEdit, WM_SETTEXT, MAX_LOADSTRING, LPARAM(ofn.lpstrFile));
		UpdateWindow(hEdit);
	}
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
	HWND hWnd = (HWND)lpParam;

	HWND hEdit1 = GetDlgItem(hWnd, ID_TEXTBOX_1);
	HWND hEdit2 = GetDlgItem(hWnd, ID_TEXTBOX_2);
	HWND hPrBar = GetDlgItem(hWnd, ID_PROGRESSBAR);
	TCHAR buff1[MAX_LOADSTRING];
	TCHAR buff2[MAX_LOADSTRING];
	SendMessage(hEdit1, WM_GETTEXT, MAX_LOADSTRING, (LPARAM)buff1);
	SendMessage(hEdit2, WM_GETTEXT, MAX_LOADSTRING, (LPARAM)buff2);

	if (compare(buff1, buff2, hPrBar)) MessageBox(hWnd, L"Файлы одинаковые!", L"Сообщение", MB_OK);

	return 0;
}

void loadPage(const WCHAR *pszFileName1, const WCHAR *pszFileName2, unsigned long long offset)
{
	HANDLE hfile1 = CreateFile(pszFileName1, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	HANDLE hfile2 = CreateFile(pszFileName2, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (hfile1 != INVALID_HANDLE_VALUE && hfile2 != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER file_size1{};
		LARGE_INTEGER file_size2{};

		GetFileSizeEx(hfile1, &file_size1);
		GetFileSizeEx(hfile2, &file_size2);

		const unsigned long long cbFile1 = static_cast<unsigned long long>(file_size1.QuadPart);
		const unsigned long long cbFile2 = static_cast<unsigned long long>(file_size2.QuadPart);

		MAXcbFile = cbFile1 > cbFile2 ? cbFile1 : cbFile2;

		HANDLE hmap1 = CreateFileMapping(hfile1, NULL, PAGE_READONLY, 0, 0, NULL);
		HANDLE hmap2 = CreateFileMapping(hfile2, NULL, PAGE_READONLY, 0, 0, NULL);
		if (hmap1 != NULL && hmap2 != NULL)
		{
			SendMessage(hList1, LB_RESETCONTENT, 0, 0);
			SendMessage(hList2, LB_RESETCONTENT, 0, 0);
			SendMessage(hList1, WM_SETREDRAW, FALSE, 0);
			SendMessage(hList2, WM_SETREDRAW, FALSE, 0);

			DWORD cbView1 = cbView;
			DWORD cbView2 = cbView;

			DWORD high = static_cast<DWORD>((offset >> 32) & 0xFFFFFFFFul);
			DWORD low = static_cast<DWORD>(offset & 0xFFFFFFFFul);

			// последняя может быть мешьше
			if (offset + cbView > cbFile1) 
			{
				cbView1 = static_cast<int>(cbFile1 - offset);
			}
			
			if(offset + cbView > cbFile2)
			{
				cbView2 = static_cast<int>(cbFile2 - offset);
			}

			const unsigned char *pView1 = NULL;
			const unsigned char *pView2 = NULL;

			if (offset + cbView1 <= cbFile1) pView1 = static_cast<const unsigned char *>(MapViewOfFile(hmap1, FILE_MAP_READ, high, low, cbView1));
			if (offset + cbView2 <= cbFile2) pView2 = static_cast<const unsigned char *>(MapViewOfFile(hmap2, FILE_MAP_READ, high, low, cbView2));

			if (pView1 != NULL || pView2 != NULL)
			{
				for (int i = 0; i < cbView; i += 8)
				{
					//СРАВНЕНИЕ
					unsigned char data = 0;
					if (pView1 != NULL && pView2 != NULL)
					{
						for (int j = 0; j < 8; j++)
						{
							int index = j + i;

							if (index >= cbView1 || index >= cbView2) break;

							if (pView1[index] == pView2[index]) data |= 1 << j;
						}
					}

					//ДОБАВЛЕНИЕ
					if (i < cbView1) AddItem(hList1, toHex(pView1, i, offset), data);
					else AddItem(hList1, toHex(NULL, i, offset), data);

					if (i < cbView2) AddItem(hList2, toHex(pView2, i, offset), data);
					else AddItem(hList2, toHex(NULL, i, offset), data);
				}
			}
			
			
			SendMessage(hList1, WM_SETREDRAW, TRUE, 0L);
			SendMessage(hList2, WM_SETREDRAW, TRUE, 0L);
			UpdateWindow(hList1);
			UpdateWindow(hList2);

			CloseHandle(hmap1);
			CloseHandle(hmap2);
		}
		CloseHandle(hfile1);
		CloseHandle(hfile2);
	}
}

void AddItem(HWND hwnd, LPCWSTR pstr, unsigned char data)
{
	int lbItem;

	lbItem = SendMessage(hwnd, LB_ADDSTRING, 0, (LPARAM)pstr);
	SendMessage(hwnd, LB_SETITEMDATA, (WPARAM)lbItem, (LPARAM)data);
}

LPWSTR toHex(const unsigned char *pView, int i, int offset)
{
	char c = '.';
	LPWSTR buff = new WCHAR[35];
	buff[34] = '\0';

	if (pView != NULL)
	{
		wsprintf(buff, L"%010X%02X%02X%02X%02X%02X%02X%02X%02X%c%c%c%c%c%c%c%c", offset + i, pView[i], pView[i + 1], pView[i + 2], pView[i + 3], pView[i + 4], pView[i + 5], pView[i + 6], pView[i + 7],
			pView[i] == 0 ? c : pView[i],
			pView[i + 1] == 0 ? c : pView[i + 1],
			pView[i + 2] == 0 ? c : pView[i + 2],
			pView[i + 3] == 0 ? c : pView[i + 3],
			pView[i + 4] == 0 ? c : pView[i + 4],
			pView[i + 5] == 0 ? c : pView[i + 5],
			pView[i + 6] == 0 ? c : pView[i + 6],
			pView[i + 7] == 0 ? c : pView[i + 7]);
	}
	else
	{
		wsprintf(buff, L"%010X                        ", offset + i);
	}

	return buff;
}

BOOL compare(const WCHAR *pszFileName1, const WCHAR *pszFileName2, HWND hPrBar)
{
	SYSTEM_INFO sysinfo{};
	GetSystemInfo(&sysinfo);
	DWORD cbView = sysinfo.dwAllocationGranularity;

	BOOL isEqual = TRUE;

	HANDLE hfile1 = CreateFile(pszFileName1, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	HANDLE hfile2 = CreateFile(pszFileName2, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (hfile1 != INVALID_HANDLE_VALUE && hfile2 != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER file_size1{};
		LARGE_INTEGER file_size2{};

		GetFileSizeEx(hfile1, &file_size1);
		GetFileSizeEx(hfile2, &file_size2);

		const unsigned long long cbFile1 = static_cast<unsigned long long>(file_size1.QuadPart);
		const unsigned long long cbFile2 = static_cast<unsigned long long>(file_size2.QuadPart);

		const unsigned long long MAXcbFile = cbFile1 > cbFile2 ? cbFile1 : cbFile2;

		SendMessage(hPrBar, PBM_SETRANGE, 0, MAKELPARAM(0, MAXcbFile / cbView));

		SendMessage(hPrBar, PBM_SETSTEP, (WPARAM)1, 0);

		HANDLE hmap1 = CreateFileMapping(hfile1, NULL, PAGE_READONLY, 0, 0, NULL);
		HANDLE hmap2 = CreateFileMapping(hfile2, NULL, PAGE_READONLY, 0, 0, NULL);
		if (hmap1 != NULL && hmap2 != NULL)
		{
			DWORD cbView1 = cbView;
			DWORD cbView2 = cbView;

			for (unsigned long long offset = 0; offset < MAXcbFile; offset += cbView)
			{
				DWORD high = static_cast<DWORD>((offset >> 32) & 0xFFFFFFFFul);
				DWORD low = static_cast<DWORD>(offset & 0xFFFFFFFFul);

				// последняя может быть мешьше
				if (offset + cbView > cbFile1)
				{
					cbView1 = static_cast<int>(cbFile1 - offset);
				}

				if (offset + cbView > cbFile2)
				{
					cbView2 = static_cast<int>(cbFile2 - offset);
				}

				if (cbView1 != cbView2)
				{
					isEqual = FALSE;
					break;
				}

				const char *pView1 = NULL;
				const char *pView2 = NULL;

				if (offset + cbView1 <= cbFile1) pView1 = static_cast<const char *>(MapViewOfFile(hmap1, FILE_MAP_READ, high, low, cbView1));
				if (offset + cbView2 <= cbFile2) pView2 = static_cast<const char *>(MapViewOfFile(hmap2, FILE_MAP_READ, high, low, cbView2));

				if (pView1 != NULL && pView2 != NULL)
				{
					if (memcmp(pView1, pView2, cbView1))
					{
						isEqual = FALSE;
						break;
					}
				}
				SendMessage(hPrBar, PBM_STEPIT, 0, 0);
			}
			UpdateWindow(hList1);
			UpdateWindow(hList2);

			CloseHandle(hmap1);
			CloseHandle(hmap2);
		}
		else isEqual = FALSE;
		CloseHandle(hfile1);
		CloseHandle(hfile2);
	}
	else isEqual = FALSE;

	SendMessage(hPrBar, PBM_SETSTEP, (WPARAM)0, 0);

	return isEqual;
}
