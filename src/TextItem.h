#pragma once
#include <windows.h>
#include <string>

enum class OutlineMode : int
{
    None = 0,
    Standard = 1,
    Thick = 2,
    Shadow = 3,
    OutlineAndShadow = 4
};

struct TextItem
{
    std::wstring text;
    std::wstring fontName = L"Segoe UI";
    float x = 0.0f;              // Image-space coordinate, in pixels
    float y = 0.0f;
    float fontSizePt = 48.0f;    // Point size
    bool bold = true;
    bool italic = false;
    bool vertical = false;         // Vertical Japanese text layout
    COLORREF fillColor = RGB(255, 255, 255);
    COLORREF outlineColor = RGB(0, 0, 0);
    COLORREF shadowColor = RGB(0, 0, 0);
    float outlineWidth = 3.0f;   // Image-space pixels
    OutlineMode outlineMode = OutlineMode::Standard;
};
