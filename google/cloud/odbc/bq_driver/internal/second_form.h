#pragma once
#include <windows.h>

class SecondForm
{
public:
    SecondForm();
    ~SecondForm();
    void Show(HWND parent);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HWND m_hwnd;
    static const char CLASS_NAME[];
};