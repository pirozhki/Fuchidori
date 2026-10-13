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

}

TextItem CTextDialog::GetCurrentItem()
{
    if (!IsWindow())
        return m_settings;

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

    m_settings = item;
    return item;
}

TextItem CTextDialog::GetDefaultItem(const std::wstring& text) const
{
    TextItem item = m_defaults;
    item.text = text;
    item.x = 0.0f;
    item.y = 0.0f;
    return item;
}

void CTextDialog::SetCurrentItem(const TextItem& item)
{
    m_settings = item;
    if (!IsWindow())
        return;

    m_updatingControls = true;
    ::SetDlgItemTextW(m_hWnd, IDC_TEXT_INPUT, item.text.c_str());

    ::SetDlgItemTextW(m_hWnd, IDC_FONT_NAME, item.fontName.c_str());
    const LRESULT fontIndex = ::SendDlgItemMessageW(m_hWnd, IDC_FONT_NAME,
        CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(item.fontName.c_str()));
    if (fontIndex != CB_ERR)
        ::SendDlgItemMessageW(m_hWnd, IDC_FONT_NAME, CB_SETCURSEL, static_cast<WPARAM>(fontIndex), 0);

    wchar_t number[64] = {};
    swprintf_s(number, L"%.1f", item.fontSizePt);
    ::SetDlgItemTextW(m_hWnd, IDC_FONT_SIZE, number);
    swprintf_s(number, L"%.1f", item.outlineWidth);
    ::SetDlgItemTextW(m_hWnd, IDC_OUTLINE_WIDTH, number);
    ::CheckDlgButton(m_hWnd, IDC_BOLD, item.bold ? BST_CHECKED : BST_UNCHECKED);
    ::CheckDlgButton(m_hWnd, IDC_ITALIC, item.italic ? BST_CHECKED : BST_UNCHECKED);
    ::CheckDlgButton(m_hWnd, IDC_VERTICAL, item.vertical ? BST_CHECKED : BST_UNCHECKED);
    ::SendDlgItemMessageW(m_hWnd, IDC_OUTLINE_MODE, CB_SETCURSEL,
        static_cast<WPARAM>(item.outlineMode), 0);
    UpdateColorButtonText(IDC_FILL_COLOR, item.fillColor);
    UpdateColorButtonText(IDC_OUTLINE_COLOR, item.outlineColor);
    UpdateColorButtonText(IDC_SHADOW_COLOR, item.shadowColor);
    m_updatingControls = false;
}

void CTextDialog::ResetToDefaults(const std::wstring& text)
{
    SetCurrentItem(GetDefaultItem(text));
}

void CTextDialog::UpdateDefaultsFromCurrentItem(const TextItem& item)
{
    m_defaults = item;
    m_defaults.text.clear();
    m_defaults.x = 0.0f;
    m_defaults.y = 0.0f;
}

void CTextDialog::UpdateActionButtons(bool hasSelection, bool hasPending)
{
    if (!IsWindow())
        return;
    ::EnableWindow(::GetDlgItem(m_hWnd, IDC_COMMIT_SELECTED), hasSelection ? TRUE : FALSE);
    ::EnableWindow(::GetDlgItem(m_hWnd, IDC_COMMIT_ALL), hasPending ? TRUE : FALSE);
    ::EnableWindow(::GetDlgItem(m_hWnd, IDC_DELETE_SELECTED), hasSelection ? TRUE : FALSE);
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
    PopulateFonts();

    const wchar_t* outlineOptions[] = {
        L"なし", L"標準の縁取り", L"太い縁取り", L"影のみ", L"縁取り＋影"
    };
    for (const wchar_t* option : outlineOptions)
        ::SendDlgItemMessageW(m_hWnd, IDC_OUTLINE_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(option));

    SetCurrentItem(m_defaults);

    // Place the tool window beside the main frame when screen space permits,
    // keeping as much of the image canvas unobstructed as possible.
    RECT ownerRect = {};
    RECT dialogRect = {};
    RECT workArea = {};
    HWND owner = GetParent();
    if (owner && ::GetWindowRect(owner, &ownerRect) && ::GetWindowRect(m_hWnd, &dialogRect))
    {
        HMONITOR monitor = ::MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (monitor && ::GetMonitorInfoW(monitor, &monitorInfo))
            workArea = monitorInfo.rcWork;
        else
            ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

        const int dialogWidth = dialogRect.right - dialogRect.left;
        const int dialogHeight = dialogRect.bottom - dialogRect.top;
        int x = ownerRect.right + 8;
        if (x + dialogWidth > workArea.right)
            x = ownerRect.left - dialogWidth - 8;
        if (x < workArea.left)
            x = workArea.right - dialogWidth;
        int y = ownerRect.top + 32;
        if (y + dialogHeight > workArea.bottom)
            y = workArea.bottom - dialogHeight;
        if (y < workArea.top)
            y = workArea.top;
        ::SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else
    {
        CenterWindow(owner);
    }

    UpdateActionButtons(false, false);
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
            current.fillColor = cc.rgbResult;
        else if (controlId == IDC_OUTLINE_COLOR)
            current.outlineColor = cc.rgbResult;
        else
            current.shadowColor = cc.rgbResult;
        SetCurrentItem(current);
        if (m_owner)
            m_owner->OnTextDialogChanged();
    }
}

LRESULT CTextDialog::OnCommand(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    if (m_updatingControls)
        return 0;

    const int id = LOWORD(wParam);
    const int notification = HIWORD(wParam);

    if (id == IDC_COMMIT_SELECTED && notification == BN_CLICKED)
    {
        if (m_owner)
            m_owner->CommitSelectedText();
        return 0;
    }
    if (id == IDC_COMMIT_ALL && notification == BN_CLICKED)
    {
        if (m_owner)
            m_owner->CommitAllPendingTexts();
        return 0;
    }
    if (id == IDC_DELETE_SELECTED && notification == BN_CLICKED)
    {
        if (m_owner)
            m_owner->DeleteSelectedObject();
        return 0;
    }
    if (id == IDCANCEL)
    {
        if (m_owner)
            m_owner->DeselectSelectedObject();
        return 0;
    }
    if (notification == BN_CLICKED &&
        (id == IDC_FILL_COLOR || id == IDC_OUTLINE_COLOR || id == IDC_SHADOW_COLOR))
    {
        ChooseColorFor(id);
        return 0;
    }

    // Edit controls, font selection, check boxes and outline-mode changes all
    // update either the selected object or the defaults for future objects.
    if (m_owner)
        m_owner->OnTextDialogChanged();
    return 0;
}

LRESULT CTextDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    // This tool window stays available throughout the editing session.
    // It is destroyed explicitly when the main frame exits.
    if (IsWindow())
        ShowWindow(SW_SHOWNORMAL);
    return 0;
}


void CTextDialog::ForceClose()
{
    if (IsWindow())
        DestroyWindow();
}
