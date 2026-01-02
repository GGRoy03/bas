#ifdef _WIN32

#include <stdbool.h>

#include "utilities.h"
#include "parsers/parser_obj.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// ==============================================
// <Memory> : PUBLIC
// ==============================================

void *OSReserve(size_t Size)
{
	void *Result = VirtualAlloc(0, Size, MEM_RESERVE, PAGE_READWRITE);
	return Result;
}

bool OSCommit(void *At, size_t Size)
{
	void *Committed = VirtualAlloc(At, Size, MEM_COMMIT, PAGE_READWRITE);
	bool  Result    = Committed != 0;
	return Result;
}

void OSRelease(void *At, size_t Size)
{
	VirtualFree(At, 0, MEM_RELEASE);
}

// ==============================================
// <Utilities>   : INTERNAL
// ==============================================


static void
Win32Sleep(DWORD Milliseconds)
{
    Sleep(Milliseconds);
}


// ==============================================
// <Entry Point> : INTERNAL
// ==============================================


static LRESULT CALLBACK
Win32MessageHandler(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    switch (Message)
    {

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    } break;

    default: break;

    }

    return DefWindowProcA(Hwnd, Message, WParam, LParam);
}


static HWND
Win32CreateWindow(int Width, int Height, HINSTANCE HInstance, int CmdShow)
{
    HWND Result = {0};

    WNDCLASSA WindowClass =
    {
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .lpfnWndProc   = Win32MessageHandler,
        .hInstance     = HInstance,
        .lpszClassName = "Toy Engine",
    };

    if (RegisterClassA(&WindowClass))
    {
        Result = CreateWindowExA(0, WindowClass.lpszClassName, "Engine Window",
                                 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 Width, Height, 0, 0, HInstance, 0);
        if (Result != 0)
        {
            ShowWindow(Result, CmdShow);
        }
    }

    return Result;
}


int WINAPI
WinMain(HINSTANCE HInstance, HINSTANCE PrevInstance, LPSTR CmdLine, int CmdShow)
{
    HWND WindowHandle = Win32CreateWindow(1920, 1080, HInstance, CmdShow);

    ParseObjFromFile(ByteStringLiteral("data/Lowpoly_tree_sample.obj"));

    BOOL Running = true;

    while (Running)
    {
        MSG Message;
        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                Running = FALSE;
            }

            TranslateMessage(&Message);
            DispatchMessageA(&Message);
        }

        Win32Sleep(8);
    }

    return 0;
}

#endif // _WIN32