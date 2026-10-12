#include "stdafx.h"
#include "TextDialog.h"
#include "MainFrame.h"

namespace
{
    COLORREF g_customColors[16] = {};

    float ReadFloatControl(HWND hwnd, int id, float fallback)
    {
        wchar_t buffer[64] = {};
        ::GetDlgItemTextW(hwnd, id, buffer, static_cast<int>(_countof(buffer)));
        if (buffer[0] == L'\0')
            return fallback;
        wchar_t* end = nullptr;
        const double value = wcstod(buffer, &end);
        if (end == buffer || !std::isfinite(value))
            return fallback;
        return static_cast<float>(value);
    }

    std::wstring ExecutableIniPath()
    {
        wchar_t path[MAX_PATH] = {};
        const DWORD len = GetModuleFileNameW(nullptr, path, _countof(path));
        if (len == 0 || len >= _countof(path))
            return L"Fuchidori.ini";

        std::wstring result(path, len);
        const size_t slash = result.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            result.resize(slash + 1);
        else
            result.clear();
        result += L"Fuchidori.ini";
        return result;
    }
}

void CTextDialog::SetPosition(float x, float y)
{
    m_x = x;
    m_y = y;
    if (m_owner)
        m_owner->OnTextDialogChanged();
}

TextItem CTextDialog::GetCurrentItem() const
{
    TextItem item = m_settings;

    wchar_t text[8192] = {};
    ::GetDlgItemTextW(m_hWnd, IDC_TEXT_INPUT, text, static_cast<int>(_countof(text)));
    item.text = text;

    wchar_t font[LF_FACESIZE * 4] = {};
    ::GetDlgItemTextW(m_hWnd, IDC_FONT_NAME, font, static_cast<int>(_countof(font)));
    if (font[0] != L'\0')
        item.fontName = font;

    item.fontSizePt = ReadFloatControl(m_hWnd, IDC_FONT_SIZE, m_settings.fontSizePt);
    item.fontSizePt = (std::max)(1.0f, (std::min)(300.0f, item.fontSizePt));
    item.outlineWidth = ReadFloatControl(m_hWnd, IDC_OUTLINE_WIDTH, m_settings.outlineWidth);
    item.outlineWidth = (std::max)(0.5f, (std::min)(20.0f, item.outlineWidth));
    item.bold = ::IsDlgButtonChecked(m_hWnd, IDC_BOLD) == BST_CHECKED;
    item.italic = ::IsDlgButtonChecked(m_hWnd, IDC_ITALIC) == BST_CHECKED;
    item.vertical = ::IsDlgButtonChecked(m_hWnd, IDC_VERTICAL) == BST_CHECKED;

    LRESULT index = ::SendDlgItemMessageW(m_hWnd, IDC_OUTLINE_MODE, CB_GETCURSEL, 0, 0);
    if (index < 0 || index > 4)
        index = static_cast<LRESULT>(m_settings.outlineMode);
    item.outlineMode = static_cast<OutlineMode>(index);
    item.x = m_x;
    item.y = m_y;
    return item;
}

std::wstring CTextDialog::GetIniPath() const
{
    return ExecutableIniPath();
}

COLORREF CTextDialog::ReadColor(const wchar_t* key, COLORREF fallback) const
{
    wchar_t buffer[32] = {};
    const std::wstring path = GetIniPath();
    wchar_t defaultHex[16] = {};
    swprintf_s(defaultHex, L"%06X", static_cast<unsigned int>(fallback & 0x00FFFFFF));
    GetPrivateProfileStringW(L"Text", key, defaultHex, buffer, _countof(buffer), path.c_str());
    wchar_t* end = nullptr;
    const unsigned long value = wcstoul(buffer, &end, 16);
    if (end == buffer)
        return fallback;
    return static_cast<COLORREF>(value & 0x00FFFFFF);
}

void CTextDialog::WriteColor(const wchar_t* key, COLORREF color) const
{
    wchar_t buffer[16] = {};
    swprintf_s(buffer, L"%06X", static_cast<unsigned int>(color & 0x00FFFFFF));
    const std::wstring path = GetIniPath();
    WritePrivateProfileStringW(L"Text", key, buffer, path.c_str());
}

void CTextDialog::LoadSettings()
{
    const std::wstring path = GetIniPath();
    wchar_t font[LF_FACESIZE * 4] = L"Segoe UI";
    GetPrivateProfileStringW(L"Text", L"FontName", L"Segoe UI", font, _countof(font), path.c_str());
    m_settings.fontName = font;

    wchar_t number[64] = {};
    GetPrivateProfileStringW(L"Text", L"FontSizePt", L"48", number, _countof(number), path.c_str());
    m_settings.fontSizePt = static_cast<float>(_wtof(number));
    if (m_settings.fontSizePt < 1.0f || m_settings.fontSizePt > 300.0f)
        m_settings.fontSizePt = 48.0f;

    m_settings.bold = GetPrivateProfileIntW(L"Text", L"Bold", 1, path.c_str()) != 0;
    m_settings.italic = GetPrivateProfileIntW(L"Text", L"Italic", 0, path.c_str()) != 0;
    m_settings.vertical = GetPrivateProfileIntW(L"Text", L"Vertical", 0, path.c_str()) != 0;
    m_settings.fillColor = ReadColor(L"FillColor", RGB(255, 255, 255));
    m_settings.outlineColor = ReadColor(L"OutlineColor", RGB(0, 0, 0));
    m_settings.shadowColor = ReadColor(L"ShadowColor", RGB(0, 0, 0));
    int outlineMode = GetPrivateProfileIntW(L"Text", L"OutlineMode", 1, path.c_str());
    outlineMode = (std::max)(0, (std::min)(4, outlineMode));
    m_settings.outlineMode = static_cast<OutlineMode>(outlineMode);

    GetPrivateProfileStringW(L"Text", L"OutlineWidth", L"3", number, _countof(number), path.c_str());
    m_settings.outlineWidth = static_cast<float>(_wtof(number));
    if (m_settings.outlineWidth < 0.5f || m_settings.outlineWidth > 20.0f)
        m_settings.outlineWidth = 3.0f;
}

int CALLBACK CTextDialog::EnumFontProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM context)
{
    if (!lf || !context)
        return 1;
    auto* self = reinterpret_cast<CTextDialog*>(context);
    const std::wstring name(lf->lfFaceName);
    if (!name.empty() && self->m_fontNames.insert(name).second)
        ::SendDlgItemMessageW(self->m_hWnd, IDC_FONT_NAME, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    return 1;
}

void CTextDialog::PopulateFonts()
{
    m_fontNames.clear();
    HDC dc = ::GetDC(m_hWnd);
    if (!dc)
        return;

    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(dc, &lf, &CTextDialog::EnumFontProc, reinterpret_cast<LPARAM>(this), 0);
    ::ReleaseDC(m_hWnd, dc);
}

void CTextDialog::UpdateColorButtonText(int controlId, COLORREF color)
{
    wchar_t label[80] = {};
    const wchar_t* prefix = L"色";
    if (controlId == IDC_FILL_COLOR)
        prefix = L"文字色";
    else if (controlId == IDC_OUTLINE_COLOR)
        prefix = L"縁取り色";
    else if (controlId == IDC_SHADOW_COLOR)
        prefix = L"影の色";

    swprintf_s(label, L"%s #%02X%02X%02X", prefix,
               static_cast<unsigned int>(GetRValue(color)),
               static_cast<unsigned int>(GetGValue(color)),
               static_cast<unsigned int>(GetBValue(color)));
    ::SetDlgItemTextW(m_hWnd, controlId, label);
}

LRESULT CTextDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
    LoadSettings();
    PopulateFonts();

    const wchar_t* outlineOptions[] = {
        L"なし", L"標準の縁取り", L"太い縁取り", L"影のみ", L"縁取り＋影"
    };
    for (const wchar_t* option : outlineOptions)
        ::SendDlgItemMessageW(m_hWnd, IDC_OUTLINE_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(option));

    ::SetDlgItemTextW(m_hWnd, IDC_FONT_NAME, m_settings.fontName.c_str());
    wchar_t number[64] = {};
    swprintf_s(number, L"%.1f", m_settings.fontSizePt);
    ::SetDlgItemTextW(m_hWnd, IDC_FONT_SIZE, number);
    swprintf_s(number, L"%.1f", m_settings.outlineWidth);
    ::SetDlgItemTextW(m_hWnd, IDC_OUTLINE_WIDTH, number);
    ::CheckDlgButton(m_hWnd, IDC_BOLD, m_settings.bold ? BST_CHECKED : BST_UNCHECKED);
    ::CheckDlgButton(m_hWnd, IDC_ITALIC, m_settings.italic ? BST_CHECKED : BST_UNCHECKED);
    ::CheckDlgButton(m_hWnd, IDC_VERTICAL, m_settings.vertical ? BST_CHECKED : BST_UNCHECKED);
    ::SendDlgItemMessageW(m_hWnd, IDC_OUTLINE_MODE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.outlineMode), 0);
    UpdateColorButtonText(IDC_FILL_COLOR, m_settings.fillColor);
    UpdateColorButtonText(IDC_OUTLINE_COLOR, m_settings.outlineColor);
    UpdateColorButtonText(IDC_SHADOW_COLOR, m_settings.shadowColor);
    // Restore the last text from memory (never from the INI file).
    ::SetDlgItemTextW(m_hWnd, IDC_TEXT_INPUT, m_lastText.c_str());
    CenterWindow(GetParent());
    return TRUE;
}

void CTextDialog::ChooseColorFor(int controlId)
{
    TextItem current = GetCurrentItem();
    COLORREF color = current.fillColor;
    if (controlId == IDC_OUTLINE_COLOR)
        color = current.outlineColor;
    else if (controlId == IDC_SHADOW_COLOR)
        color = current.shadowColor;

    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = m_hWnd;
    cc.rgbResult = color;
    cc.lpCustColors = g_customColors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&cc))
    {
        if (controlId == IDC_FILL_COLOR)
            m_settings.fillColor = cc.rgbResult;
        else if (controlId == IDC_OUTLINE_COLOR)
            m_settings.outlineColor = cc.rgbResult;
        else
            m_settings.shadowColor = cc.rgbResult;

        UpdateColorButtonText(controlId, cc.rgbResult);
        if (m_owner)
            m_owner->OnTextDialogChanged();
    }
}

LRESULT CTextDialog::OnCommand(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    const int id = LOWORD(wParam);
    const int notification = HIWORD(wParam);

    if (id == IDOK)
    {
        CloseDialog(true);
        return 0;
    }
    if (id == IDCANCEL)
    {
        CloseDialog(false);
        return 0;
    }
    if (id == IDC_CLEAR_TEXT && notification == BN_CLICKED)
    {
        ::SetDlgItemTextW(m_hWnd, IDC_TEXT_INPUT, L"");
        m_lastText.clear();
        if (m_owner)
            m_owner->OnTextDialogChanged();
        ::SetFocus(::GetDlgItem(m_hWnd, IDC_TEXT_INPUT));
        return 0;
    }
    if (notification == BN_CLICKED &&
        (id == IDC_FILL_COLOR || id == IDC_OUTLINE_COLOR || id == IDC_SHADOW_COLOR))
    {
        ChooseColorFor(id);
        return 0;
    }

    if (id == IDC_FONT_NAME && notification == CBN_SELCHANGE)
    {
        // Selection changes are handled like other preview-affecting settings.
    }
    if (m_owner)
        m_owner->OnTextDialogChanged();
    return 0;
}

LRESULT CTextDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    CloseDialog(false);
    return 0;
}

void CTextDialog::SaveSettings()
{
    if (IsWindow())
        m_settings = GetCurrentItem();

    const std::wstring path = GetIniPath();
    WritePrivateProfileStringW(L"Text", L"FontName", m_settings.fontName.c_str(), path.c_str());
    wchar_t number[64] = {};
    swprintf_s(number, L"%.2f", m_settings.fontSizePt);
    WritePrivateProfileStringW(L"Text", L"FontSizePt", number, path.c_str());
    WritePrivateProfileStringW(L"Text", L"Bold", m_settings.bold ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"Text", L"Italic", m_settings.italic ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"Text", L"Vertical", m_settings.vertical ? L"1" : L"0", path.c_str());
    WriteColor(L"FillColor", m_settings.fillColor);
    WriteColor(L"OutlineColor", m_settings.outlineColor);
    WriteColor(L"ShadowColor", m_settings.shadowColor);
    swprintf_s(number, L"%d", static_cast<int>(m_settings.outlineMode));
    WritePrivateProfileStringW(L"Text", L"OutlineMode", number, path.c_str());
    swprintf_s(number, L"%.2f", m_settings.outlineWidth);
    WritePrivateProfileStringW(L"Text", L"OutlineWidth", number, path.c_str());
}

void CTextDialog::CloseDialog(bool accepted)
{
    if (!IsWindow())
        return;

    TextItem item = GetCurrentItem();
    // Preserve the typed text across dialog openings, even after Undo or Cancel.
    // This value is session-only and is not persisted to the INI file.
    m_lastText = item.text;
    SaveSettings();
    if (accepted && m_owner && !item.text.empty())
        m_owner->OnTextAccepted(item, m_x, m_y);

    HWND oldWindow = m_hWnd;
    DestroyWindow();
    if (m_owner)
        m_owner->OnTextDialogClosed(this);
    (void)oldWindow;
}

void CTextDialog::ForceClose()
{
    if (IsWindow())
        CloseDialog(false);
}
