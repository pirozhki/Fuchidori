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

    TextItem GetCurrentItem();
    TextItem GetDefaultItem(const std::wstring& text = L"") const;
    void SetCurrentItem(const TextItem& item);
    void ResetToDefaults(const std::wstring& text);
    void UpdateDefaultsFromCurrentItem(const TextItem& item);
    void UpdateActionButtons(bool hasSelection, bool hasPending);
    void ForceClose();
    bool IsUpdatingControls() const { return m_updatingControls; }

    BEGIN_MSG_MAP(CTextDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
    END_MSG_MAP()

private:
    static int CALLBACK EnumFontProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM context);
    void PopulateFonts();
    void UpdateColorButtonText(int controlId, COLORREF color);
    void ChooseColorFor(int controlId);

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnCommand(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);

private:
    CImageTextFrame* m_owner = nullptr;
    TextItem m_defaults;
    TextItem m_settings;
    bool m_updatingControls = false;
    std::set<std::wstring> m_fontNames;
};
