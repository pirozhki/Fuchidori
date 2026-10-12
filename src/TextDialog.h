#pragma once
#include "stdafx.h"
#include "TextItem.h"
#include "resource.h"

class CImageTextFrame;

class CTextDialog : public CDialogImpl<CTextDialog>
{
public:
    enum { IDD = IDD_TEXT_DIALOG };

    explicit CTextDialog(CImageTextFrame* owner = nullptr) : m_owner(owner) {}
    void SetOwner(CImageTextFrame* owner) { m_owner = owner; }
    void SetPosition(float x, float y);
    TextItem GetCurrentItem() const;
    void SaveSettings();
    void ForceClose();

    BEGIN_MSG_MAP(CTextDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
    END_MSG_MAP()

private:
    static int CALLBACK EnumFontProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM context);
    void PopulateFonts();
    void LoadSettings();
    void UpdateColorButtonText(int controlId, COLORREF color);
    void ChooseColorFor(int controlId);
    void CloseDialog(bool accepted);
    std::wstring GetIniPath() const;
    COLORREF ReadColor(const wchar_t* key, COLORREF fallback) const;
    void WriteColor(const wchar_t* key, COLORREF color) const;

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCommand(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);

private:
    CImageTextFrame* m_owner = nullptr;
    TextItem m_settings;
    // Text draft is kept only for the lifetime of this application session.
    // It is deliberately not written to the INI file.
    std::wstring m_lastText;
    float m_x = 0.0f;
    float m_y = 0.0f;
    std::set<std::wstring> m_fontNames;
};
