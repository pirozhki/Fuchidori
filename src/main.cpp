#include "stdafx.h"
#include "MainFrame.h"

CAppModule _Module;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        MessageBoxW(nullptr, L"COM の初期化に失敗しました。", L"Fuchidori - 起動エラー", MB_ICONERROR | MB_OK);
        return 1;
    }

    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);
    SetProcessDPIAware();

    hr = _Module.Init(nullptr, hInstance);
    if (FAILED(hr))
    {
        CoUninitialize();
        MessageBoxW(nullptr, L"WTL の初期化に失敗しました。", L"Fuchidori - 起動エラー", MB_ICONERROR | MB_OK);
        return 1;
    }

    int result = 1;
    {
        CMessageLoop messageLoop;
        _Module.AddMessageLoop(&messageLoop);
        {
            CImageTextFrame frame;
            HWND window = frame.CreateEx();
            if (window)
            {
                ::SetWindowTextW(window, L"Fuchidori");
                messageLoop.AddMessageFilter(&frame);
                frame.ShowWindow(nCmdShow);
                frame.UpdateWindow();
                result = messageLoop.Run();
            }
            else
            {
                MessageBoxW(nullptr, L"メインウィンドウを作成できませんでした。", L"Fuchidori - 起動エラー", MB_ICONERROR | MB_OK);
            }
        } // All Direct2D/WIC COM objects are released before COM is uninitialized.
        _Module.RemoveMessageLoop();
    }

    _Module.Term();
    CoUninitialize();
    return result;
}
