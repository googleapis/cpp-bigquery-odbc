#include "second_form.h"
#include <windows.h>

const char SecondForm::CLASS_NAME[] = "SecondFormClass";

// Define control IDs
#define IDC_CHECKBOX 101
#define IDC_BUTTON_OK 102
#define IDC_BUTTON_CANCEL 103

SecondForm::SecondForm() : m_hwnd(NULL)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = SecondForm::WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);
}

SecondForm::~SecondForm()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
    }
    UnregisterClass(CLASS_NAME, GetModuleHandle(NULL));
}

void SecondForm::Show(HWND parent)
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        return;
    }

    m_hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Second Form",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        parent,
        NULL,
        GetModuleHandle(NULL),
        this
    );

    if (m_hwnd)
    {
        // Create a checkbox
        CreateWindowEx(
            0,
            "BUTTON",
            "Check me",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 20, 100, 30,
            m_hwnd,
            (HMENU)IDC_CHECKBOX,
            GetModuleHandle(NULL),
            NULL
        );

        // Create an OK button
        CreateWindowEx(
            0,
            "BUTTON",
            "OK",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 60, 100, 30,
            m_hwnd,
            (HMENU)IDC_BUTTON_OK,
            GetModuleHandle(NULL),
            NULL
        );

        // Create a Cancel button
        CreateWindowEx(
            0,
            "BUTTON",
            "Cancel",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            140, 60, 100, 30,
            m_hwnd,
            (HMENU)IDC_BUTTON_CANCEL,
            GetModuleHandle(NULL),
            NULL
        );

        ShowWindow(m_hwnd, SW_SHOW);
    }
}

LRESULT CALLBACK SecondForm::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SecondForm* pThis = NULL;

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (SecondForm*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = (SecondForm*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    switch (uMsg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_OK:
            MessageBox(hwnd, "OK Button Clicked", "Info", MB_OK);
            break;
        case IDC_BUTTON_CANCEL:
            MessageBox(hwnd, "Cancel Button Clicked", "Info", MB_OK);
            break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        pThis->m_hwnd = NULL;
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}