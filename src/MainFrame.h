#pragma once
#include "stdafx.h"
#include "TextItem.h"
#include "TextDialog.h"
#include "resource.h"

struct PendingTextObject
{
    ULONGLONG id = 0;
    TextItem item;
};

struct EditorState
{
    std::vector<TextItem> committedTextItems;
    std::vector<PendingTextObject> pendingObjects;
    ULONGLONG selectedObjectId = 0;
};

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
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(WM_RBUTTONUP, OnRButtonUp)
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
    void CommitSelectedText();
    void CommitAllPendingTexts();
    void DeselectSelectedObject();
    void DeleteSelectedObject();

private:
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBackground(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnRButtonUp(UINT, WPARAM, LPARAM, BOOL&);
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
    bool ConfirmDiscardPendingObjects();

    void DrawScene(ID2D1RenderTarget* target);
    HRESULT CreateTextLayoutForItem(const TextItem& item, float displayScale,
        float offsetX, float offsetY, CComPtr<IDWriteTextLayout>& layout,
        D2D1_POINT_2F& origin) const;
    void DrawTextItem(ID2D1RenderTarget* target, const TextItem& item,
                      float displayScale, float offsetX, float offsetY);
    void DrawPendingObject(ID2D1RenderTarget* target, const PendingTextObject& object,
                           float displayScale, float offsetX, float offsetY);
    bool GetTextObjectScreenRect(const TextItem& item, float displayScale,
                                 float offsetX, float offsetY, D2D1_RECT_F& rect) const;
    D2D1_RECT_F CalculateImageRect(float clientWidth, float clientHeight, float& scale) const;
    bool ClientToImagePoint(float clientX, float clientY, float& imageX, float& imageY,
                            bool requireInsideImage) const;
    int HitTestPendingObject(float clientX, float clientY, float displayScale,
                             float offsetX, float offsetY) const;
    int FindPendingObjectIndex(ULONGLONG id) const;
    bool HasPendingObjects() const;
    void UpdateDialogButtons();
    void SyncDialogToSelection();
    void PushUndoState();
    EditorState CaptureState() const;
    void RestoreState(const EditorState& state);
    void Undo();
    void Redo();

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

    // Committed text is rendered like image content and has no editable object frame.
    std::vector<TextItem> m_committedTextItems;
    // Pending objects remain individually selectable and editable until committed.
    std::vector<PendingTextObject> m_pendingObjects;
    std::vector<EditorState> m_undoStack;
    std::vector<EditorState> m_redoStack;
    ULONGLONG m_selectedObjectId = 0;
    ULONGLONG m_nextObjectId = 1;

    CTextDialog m_textDialog;

    ULONGLONG m_draggingObjectId = 0;
    bool m_dragUndoStateSaved = false;
    POINT m_dragStartClient = {};
    float m_dragOffsetX = 0.0f;
    float m_dragOffsetY = 0.0f;
};
