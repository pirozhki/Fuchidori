#pragma once
#include "stdafx.h"
#include "TextItem.h"
#include "TextDialog.h"
#include "resource.h"

class CImageTextFrame : public CFrameWindowImpl<CImageTextFrame>, public CMessageFilter
{
public:
    DECLARE_FRAME_WND_CLASS(L"FuchidoriWindow", IDR_MAINFRAME)

    CImageTextFrame() : m_textDialog(this) {}

    BOOL PreTranslateMessage(MSG* pMsg) override;

    BEGIN_MSG_MAP(CImageTextFrame)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_INITMENUPOPUP, OnInitMenuPopup)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
        COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
        COMMAND_ID_HANDLER(ID_FILE_SAVEAS, OnFileSaveAs)
        COMMAND_ID_HANDLER(ID_FILE_EXIT, OnFileExit)
        COMMAND_ID_HANDLER(ID_EDIT_UNDO, OnUndo)
        COMMAND_ID_HANDLER(ID_EDIT_REDO, OnRedo)
        CHAIN_MSG_MAP(CFrameWindowImpl<CImageTextFrame>)
    END_MSG_MAP()

    void OnTextDialogChanged();
    void OnTextDialogClosed(CTextDialog* dialog);
    void OnTextAccepted(const TextItem& item, float x, float y);

private:
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBackground(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnInitMenuPopup(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDropFiles(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnFileOpen(WORD, WORD, HWND, BOOL&);
    LRESULT OnFileSaveAs(WORD, WORD, HWND, BOOL&);
    LRESULT OnFileExit(WORD, WORD, HWND, BOOL&);
    LRESULT OnUndo(WORD, WORD, HWND, BOOL&);
    LRESULT OnRedo(WORD, WORD, HWND, BOOL&);

    HRESULT InitializeGraphics();
    HRESULT EnsureRenderTarget();
    HRESULT LoadImageFile(const std::wstring& path);
    HRESULT SaveFlattenedImage(const std::wstring& path);
    bool ShowOpenDialog(std::wstring& path);
    bool ShowSaveDialog(std::wstring& path);
    void DrawScene(ID2D1RenderTarget* target, bool includePreview);
    void DrawTextItem(ID2D1RenderTarget* target, const TextItem& item,
                      float displayScale, float offsetX, float offsetY);
    D2D1_RECT_F CalculateImageRect(float clientWidth, float clientHeight, float& scale) const;
    void CommitText(const TextItem& item);
    void Undo();
    void Redo();
    void CloseEditorWithoutCommit();

private:
    CComPtr<ID2D1Factory> m_d2dFactory;
    CComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    CComPtr<IDWriteFactory> m_dwriteFactory;
    CComPtr<IWICImagingFactory> m_wicFactory;
    CComPtr<IWICBitmap> m_wicImage;
    CComPtr<ID2D1Bitmap> m_imageBitmap;

    UINT m_imageWidth = 0;
    UINT m_imageHeight = 0;
    std::wstring m_imagePath;
    std::vector<TextItem> m_textItems;
    std::vector<std::vector<TextItem>> m_undoStack;
    std::vector<std::vector<TextItem>> m_redoStack;
    CTextDialog m_textDialog;
    float m_previewX = 0.0f;
    float m_previewY = 0.0f;
};
