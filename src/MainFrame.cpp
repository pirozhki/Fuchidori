#include "stdafx.h"
#include "MainFrame.h"

namespace
{
    D2D1_COLOR_F ToD2DColor(COLORREF color, float alpha = 1.0f)
    {
        return D2D1::ColorF(
            static_cast<float>(GetRValue(color)) / 255.0f,
            static_cast<float>(GetGValue(color)) / 255.0f,
            static_cast<float>(GetBValue(color)) / 255.0f,
            alpha);
    }

    std::wstring LowercaseExtension(const std::wstring& path)
    {
        const size_t dot = path.find_last_of(L'.');
        if (dot == std::wstring::npos)
            return L".png";
        std::wstring ext = path.substr(dot);
        for (wchar_t& ch : ext)
            ch = static_cast<wchar_t>(towlower(ch));
        return ext;
    }

    bool IsSupportedImagePath(const std::wstring& path)
    {
        const size_t slash = path.find_last_of(L"\\/");
        const size_t dot = path.find_last_of(L'.');
        if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash))
            return false;

        std::wstring ext = path.substr(dot);
        for (wchar_t& ch : ext)
            ch = static_cast<wchar_t>(towlower(ch));

        return ext == L".png" || ext == L".jpg" || ext == L".jpeg" ||
               ext == L".bmp" || ext == L".gif" || ext == L".tif" ||
               ext == L".tiff";
    }

    bool SameTextItem(const TextItem& a, const TextItem& b)
    {
        return a.text == b.text && a.fontName == b.fontName &&
            a.fontSizePt == b.fontSizePt && a.bold == b.bold &&
            a.italic == b.italic && a.vertical == b.vertical &&
            a.fillColor == b.fillColor && a.outlineColor == b.outlineColor &&
            a.shadowColor == b.shadowColor && a.outlineWidth == b.outlineWidth &&
            a.outlineMode == b.outlineMode;
    }
}

BOOL CImageTextFrame::PreTranslateMessage(MSG* pMsg)
{
    if (!pMsg || pMsg->message != WM_KEYDOWN)
        return FALSE;

    const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool dialogHasFocus = m_textDialog.IsWindow() &&
        (pMsg->hwnd == m_textDialog.m_hWnd || ::IsChild(m_textDialog.m_hWnd, pMsg->hwnd));

    if (ctrlDown && !dialogHasFocus)
    {
        if (pMsg->wParam == 'Z')
        {
            Undo();
            return TRUE;
        }
        if (pMsg->wParam == 'Y')
        {
            Redo();
            return TRUE;
        }
    }

    if (pMsg->wParam == VK_DELETE && !dialogHasFocus && m_selectedObjectId != 0)
    {
        DeleteSelectedObject();
        return TRUE;
    }

    if (pMsg->wParam == VK_ESCAPE && m_selectedObjectId != 0)
    {
        DeselectSelectedObject();
        return TRUE;
    }
    return FALSE;
}

LRESULT CImageTextFrame::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
    HRESULT hr = InitializeGraphics();
    if (SUCCEEDED(hr))
        hr = EnsureRenderTarget();
    if (FAILED(hr))
    {
        MessageBoxW(L"Direct2D / DirectWrite / WIC の初期化に失敗しました。", L"エラー", MB_ICONERROR | MB_OK);
        return -1;
    }

    // Enable the standard Shell drag-and-drop mechanism for files.
    ::DragAcceptFiles(m_hWnd, TRUE);

    // The modeless text-properties window stays open for the entire session.
    HWND dialog = m_textDialog.Create(m_hWnd);
    if (!dialog)
    {
        MessageBoxW(L"文字設定ダイアログを作成できませんでした。", L"Fuchidori", MB_ICONERROR | MB_OK);
        return -1;
    }
    m_textDialog.ShowWindow(SW_SHOWNORMAL);
    m_textDialog.UpdateActionButtons(false, false);
    return 0;
}

HRESULT CImageTextFrame::InitializeGraphics()
{
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   __uuidof(ID2D1Factory), nullptr,
                                   reinterpret_cast<void**>(&m_d2dFactory));
    if (FAILED(hr))
        return hr;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(&m_dwriteFactory));
    if (FAILED(hr))
        return hr;

    return CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                            IID_PPV_ARGS(&m_wicFactory));
}

HRESULT CImageTextFrame::EnsureRenderTarget()
{
    if (m_renderTarget)
        return S_OK;

    RECT rc = {};
    GetClientRect(&rc);
    UINT width = static_cast<UINT>((std::max)(1L, rc.right - rc.left));
    UINT height = static_cast<UINT>((std::max)(1L, rc.bottom - rc.top));

    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hWnd, D2D1::SizeU(width, height)),
        &m_renderTarget);
    if (FAILED(hr))
        return hr;

    if (m_wicImage)
    {
        hr = m_renderTarget->CreateBitmapFromWicBitmap(m_wicImage, &m_imageBitmap);
        if (FAILED(hr))
        {
            m_renderTarget.Release();
            return hr;
        }
    }
    return S_OK;
}

LRESULT CImageTextFrame::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    if (m_renderTarget)
    {
        RECT rc = {};
        GetClientRect(&rc);
        const UINT width = static_cast<UINT>((std::max)(0L, rc.right - rc.left));
        const UINT height = static_cast<UINT>((std::max)(0L, rc.bottom - rc.top));
        if (width > 0 && height > 0)
        {
            const HRESULT hr = m_renderTarget->Resize(D2D1::SizeU(width, height));
            if (FAILED(hr))
            {
                m_imageBitmap.Release();
                m_renderTarget.Release();
            }
        }
    }
    Invalidate(FALSE);
    return 0;
}

LRESULT CImageTextFrame::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps = {};
    BeginPaint(&ps);
    HRESULT hr = EnsureRenderTarget();
    if (SUCCEEDED(hr))
    {
        m_renderTarget->BeginDraw();
        DrawScene(m_renderTarget);
        hr = m_renderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            m_imageBitmap.Release();
            m_renderTarget.Release();
            Invalidate(FALSE);
        }
    }
    EndPaint(&ps);
    return 0;
}

LRESULT CImageTextFrame::OnEraseBackground(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;
}

D2D1_RECT_F CImageTextFrame::CalculateImageRect(float clientWidth, float clientHeight, float& scale) const
{
    scale = 1.0f;
    if (m_imageWidth == 0 || m_imageHeight == 0 || clientWidth <= 0.0f || clientHeight <= 0.0f)
        return D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);

    const float sx = clientWidth / static_cast<float>(m_imageWidth);
    const float sy = clientHeight / static_cast<float>(m_imageHeight);
    scale = (std::min)(sx, sy);
    const float width = static_cast<float>(m_imageWidth) * scale;
    const float height = static_cast<float>(m_imageHeight) * scale;
    const float left = (clientWidth - width) * 0.5f;
    const float top = (clientHeight - height) * 0.5f;
    return D2D1::RectF(left, top, left + width, top + height);
}

void CImageTextFrame::DrawScene(ID2D1RenderTarget* target)
{
    if (!target)
        return;

    target->Clear(D2D1::ColorF(0.13f, 0.14f, 0.16f));
    if (!m_imageBitmap || m_imageWidth == 0 || m_imageHeight == 0)
    {
        CComPtr<ID2D1SolidColorBrush> brush;
        CComPtr<IDWriteTextFormat> format;
        if (SUCCEEDED(target->CreateSolidColorBrush(D2D1::ColorF(0.88f, 0.89f, 0.91f), &brush)) &&
            SUCCEEDED(m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"ja-JP", &format)))
        {
            RECT rc = {};
            GetClientRect(&rc);
            const wchar_t* message = L"[ファイル] → [画像を開く] から画像を読み込んでください。\n文字設定ダイアログで文字を入力しておき、画像上をクリックすると配置できます。";
            target->DrawText(message, static_cast<UINT32>(wcslen(message)), format,
                D2D1::RectF(24.0f, 24.0f, static_cast<float>(rc.right - rc.left) - 24.0f,
                            static_cast<float>(rc.bottom - rc.top) - 24.0f), brush);
        }
        return;
    }

    RECT client = {};
    GetClientRect(&client);
    float scale = 1.0f;
    const D2D1_RECT_F imageRect = CalculateImageRect(
        static_cast<float>(client.right - client.left),
        static_cast<float>(client.bottom - client.top), scale);
    target->DrawBitmap(m_imageBitmap, imageRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

    // Confirmed text is rendered as part of the image and has no editing frame.
    for (const TextItem& item : m_committedTextItems)
        DrawTextItem(target, item, scale, imageRect.left, imageRect.top);

    // Pending text stays as an individual object until it is committed.
    for (const PendingTextObject& object : m_pendingObjects)
        DrawPendingObject(target, object, scale, imageRect.left, imageRect.top);
}

HRESULT CImageTextFrame::CreateTextLayoutForItem(const TextItem& item, float displayScale,
    float offsetX, float offsetY, CComPtr<IDWriteTextLayout>& layout,
    D2D1_POINT_2F& origin) const
{
    if (!m_dwriteFactory)
        return E_FAIL;

    const float fontSize = (std::max)(1.0f, item.fontSizePt * (96.0f / 72.0f) * displayScale);
    const DWRITE_FONT_WEIGHT weight = item.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    const DWRITE_FONT_STYLE style = item.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

    CComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        item.fontName.empty() ? L"Segoe UI" : item.fontName.c_str(), nullptr,
        weight, style, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"ja-JP", &format);
    if (FAILED(hr))
        return hr;

    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    float layoutWidth = 100000.0f;
    origin = D2D1::Point2F(offsetX + item.x * displayScale, offsetY + item.y * displayScale);
    if (item.vertical)
    {
        if (FAILED(format->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM)) ||
            FAILED(format->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT)))
            return E_FAIL;

        size_t columnCount = 1;
        for (size_t i = 0; i < item.text.size(); ++i)
        {
            if (item.text[i] == L'\n' ||
                (item.text[i] == L'\r' && (i + 1 == item.text.size() || item.text[i + 1] != L'\n')))
                ++columnCount;
        }
        layoutWidth = fontSize * static_cast<float>(columnCount);
        origin.x -= fontSize * static_cast<float>(columnCount - 1);
    }

    return m_dwriteFactory->CreateTextLayout(
        item.text.c_str(), static_cast<UINT32>(item.text.size()), format,
        layoutWidth, 100000.0f, &layout);
}

void CImageTextFrame::DrawTextItem(ID2D1RenderTarget* target, const TextItem& item,
                                   float displayScale, float offsetX, float offsetY)
{
    if (!target || item.text.empty() || !m_dwriteFactory)
        return;

    CComPtr<IDWriteTextLayout> layout;
    D2D1_POINT_2F origin = {};
    if (FAILED(CreateTextLayoutForItem(item, displayScale, offsetX, offsetY, layout, origin)))
        return;

    CComPtr<ID2D1SolidColorBrush> fillBrush;
    if (FAILED(target->CreateSolidColorBrush(ToD2DColor(item.fillColor), &fillBrush)))
        return;

    CComPtr<ID2D1SolidColorBrush> outlineBrush;
    CComPtr<ID2D1SolidColorBrush> shadowBrush;
    const bool hasOutline = item.outlineMode == OutlineMode::Standard ||
                            item.outlineMode == OutlineMode::Thick ||
                            item.outlineMode == OutlineMode::OutlineAndShadow;
    const bool hasShadow = item.outlineMode == OutlineMode::Shadow ||
                           item.outlineMode == OutlineMode::OutlineAndShadow;

    if (hasOutline && FAILED(target->CreateSolidColorBrush(ToD2DColor(item.outlineColor), &outlineBrush)))
        return;
    if (hasShadow && FAILED(target->CreateSolidColorBrush(ToD2DColor(item.shadowColor, 0.92f), &shadowBrush)))
        return;

    const float x = origin.x;
    const float y = origin.y;
    if (hasShadow && shadowBrush)
    {
        const float shadowOffset = (std::max)(2.0f, item.outlineWidth * 1.25f) * displayScale;
        target->DrawTextLayout(D2D1::Point2F(x + shadowOffset, y + shadowOffset), layout, shadowBrush);
    }

    if (hasOutline && outlineBrush)
    {
        float radius = item.outlineWidth * displayScale;
        if (item.outlineMode == OutlineMode::Thick)
            radius *= 1.7f;
        const int steps = 28;
        for (int i = 0; i < steps; ++i)
        {
            const float angle = (2.0f * 3.14159265358979323846f * static_cast<float>(i)) /
                                static_cast<float>(steps);
            target->DrawTextLayout(D2D1::Point2F(x + std::cos(angle) * radius,
                y + std::sin(angle) * radius), layout, outlineBrush);
        }
    }
    target->DrawTextLayout(D2D1::Point2F(x, y), layout, fillBrush);
}

bool CImageTextFrame::GetTextObjectScreenRect(const TextItem& item, float displayScale,
    float offsetX, float offsetY, D2D1_RECT_F& rect) const
{
    const float originX = offsetX + item.x * displayScale;
    const float originY = offsetY + item.y * displayScale;
    if (item.text.empty())
    {
        // Empty text objects remain visible and selectable using a compact placeholder frame.
        constexpr float width = 32.0f;
        constexpr float height = 22.0f;
        rect = D2D1::RectF(originX, originY, originX + width, originY + height);
        return true;
    }

    CComPtr<IDWriteTextLayout> layout;
    D2D1_POINT_2F origin = {};
    if (FAILED(CreateTextLayoutForItem(item, displayScale, offsetX, offsetY, layout, origin)))
        return false;

    DWRITE_TEXT_METRICS metrics = {};
    if (FAILED(layout->GetMetrics(&metrics)))
        return false;

    const float fontPixels = (std::max)(1.0f, item.fontSizePt * (96.0f / 72.0f) * displayScale);
    float contentLeft = origin.x + metrics.left;
    float contentTop = origin.y + metrics.top;
    float contentWidth = (std::max)(fontPixels * 0.5f, metrics.widthIncludingTrailingWhitespace);
    float contentHeight = (std::max)(fontPixels, metrics.height);

    if (item.vertical)
    {
        // DirectWrite's vertical layout flows columns from right to left. Its reported
        // text width can describe only the measured glyph area, while our layout reserves
        // one em-wide column for each newline-separated line. Use both measurements so
        // the editing frame always covers the complete vertical layout.
        size_t columnCount = 1;
        size_t currentColumnLength = 0;
        size_t longestColumnLength = 0;
        for (size_t i = 0; i < item.text.size(); ++i)
        {
            const wchar_t ch = item.text[i];
            if (ch == L'\r' || ch == L'\n')
            {
                longestColumnLength = (std::max)(longestColumnLength, currentColumnLength);
                currentColumnLength = 0;
                ++columnCount;
                if (ch == L'\r' && i + 1 < item.text.size() && item.text[i + 1] == L'\n')
                    ++i; // Treat CRLF as one line break.
            }
            else
            {
                ++currentColumnLength;
            }
        }
        longestColumnLength = (std::max)(longestColumnLength, currentColumnLength);

        const float reservedWidth = fontPixels * static_cast<float>(columnCount);
        const float measuredLeft = metrics.left;
        const float measuredRight = metrics.left + metrics.widthIncludingTrailingWhitespace;
        const float relativeLeft = (std::min)(0.0f, measuredLeft);
        const float relativeRight = (std::max)(reservedWidth, measuredRight);
        contentLeft = origin.x + relativeLeft;
        contentWidth = (std::max)(fontPixels * 0.5f, relativeRight - relativeLeft);

        // Keep the frame tall enough for the longest column even if DirectWrite's
        // reported metrics are narrower than the visible vertical glyph run.
        contentHeight = (std::max)(contentHeight,
            fontPixels * static_cast<float>(longestColumnLength));
    }

    const bool hasOutline = item.outlineMode == OutlineMode::Standard ||
        item.outlineMode == OutlineMode::Thick ||
        item.outlineMode == OutlineMode::OutlineAndShadow;
    const bool hasShadow = item.outlineMode == OutlineMode::Shadow ||
        item.outlineMode == OutlineMode::OutlineAndShadow;

    float outlinePad = hasOutline ? item.outlineWidth * displayScale : 0.0f;
    if (hasOutline && item.outlineMode == OutlineMode::Thick)
        outlinePad *= 1.7f;
    const float shadowOffset = hasShadow
        ? (std::max)(2.0f, item.outlineWidth * 1.25f) * displayScale : 0.0f;
    const float pad = (std::max)(4.0f, outlinePad + 2.0f);

    // The frame includes the measured layout, outline radius, and the shadow's
    // down/right offset, so selection and hit testing agree with what is drawn.
    rect = D2D1::RectF(
        contentLeft - pad,
        contentTop - pad,
        contentLeft + contentWidth + pad + shadowOffset,
        contentTop + contentHeight + pad + shadowOffset);
    return true;
}

void CImageTextFrame::DrawPendingObject(ID2D1RenderTarget* target,
    const PendingTextObject& object, float displayScale, float offsetX, float offsetY)
{
    if (!target)
        return;

    DrawTextItem(target, object.item, displayScale, offsetX, offsetY);

    D2D1_RECT_F rect = {};
    if (!GetTextObjectScreenRect(object.item, displayScale, offsetX, offsetY, rect))
        return;

    CComPtr<ID2D1SolidColorBrush> haloBrush;
    CComPtr<ID2D1SolidColorBrush> frameBrush;
    const bool selected = object.id == m_selectedObjectId;
    if (SUCCEEDED(target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f), &haloBrush)))
        target->DrawRectangle(rect, haloBrush, 3.0f);
    const D2D1_COLOR_F frameColor = selected ? D2D1::ColorF(D2D1::ColorF::Red) : D2D1::ColorF(D2D1::ColorF::Black);
    if (SUCCEEDED(target->CreateSolidColorBrush(frameColor, &frameBrush)))
        target->DrawRectangle(rect, frameBrush, selected ? 2.0f : 1.5f);
}

bool CImageTextFrame::ClientToImagePoint(float clientX, float clientY,
    float& imageX, float& imageY, bool requireInsideImage) const
{
    if (m_imageWidth == 0 || m_imageHeight == 0)
        return false;
    RECT client = {};
    GetClientRect(&client);
    float scale = 1.0f;
    const D2D1_RECT_F imageRect = CalculateImageRect(
        static_cast<float>(client.right - client.left),
        static_cast<float>(client.bottom - client.top), scale);
    if (scale <= 0.0f)
        return false;
    if (requireInsideImage && (clientX < imageRect.left || clientX >= imageRect.right ||
        clientY < imageRect.top || clientY >= imageRect.bottom))
        return false;
    imageX = (clientX - imageRect.left) / scale;
    imageY = (clientY - imageRect.top) / scale;
    imageX = (std::max)(0.0f, (std::min)(imageX, static_cast<float>(m_imageWidth - 1)));
    imageY = (std::max)(0.0f, (std::min)(imageY, static_cast<float>(m_imageHeight - 1)));
    return true;
}

int CImageTextFrame::HitTestPendingObject(float clientX, float clientY,
    float displayScale, float offsetX, float offsetY) const
{
    for (int i = static_cast<int>(m_pendingObjects.size()) - 1; i >= 0; --i)
    {
        D2D1_RECT_F rect = {};
        if (GetTextObjectScreenRect(m_pendingObjects[static_cast<size_t>(i)].item,
            displayScale, offsetX, offsetY, rect) && clientX >= rect.left && clientX <= rect.right &&
            clientY >= rect.top && clientY <= rect.bottom)
            return i;
    }
    return -1;
}

int CImageTextFrame::FindPendingObjectIndex(ULONGLONG id) const
{
    if (id == 0)
        return -1;
    for (size_t i = 0; i < m_pendingObjects.size(); ++i)
        if (m_pendingObjects[i].id == id)
            return static_cast<int>(i);
    return -1;
}

bool CImageTextFrame::HasPendingObjects() const
{
    return !m_pendingObjects.empty();
}

void CImageTextFrame::UpdateDialogButtons()
{
    const bool hasSelection = FindPendingObjectIndex(m_selectedObjectId) >= 0;
    m_textDialog.UpdateActionButtons(hasSelection, HasPendingObjects());
}

LRESULT CImageTextFrame::OnLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    float imageX = 0.0f, imageY = 0.0f;
    const float px = static_cast<float>(GET_X_LPARAM(lParam));
    const float py = static_cast<float>(GET_Y_LPARAM(lParam));
    if (!ClientToImagePoint(px, py, imageX, imageY, true))
        return 0;

    RECT client = {};
    GetClientRect(&client);
    float scale = 1.0f;
    const D2D1_RECT_F imageRect = CalculateImageRect(
        static_cast<float>(client.right - client.left),
        static_cast<float>(client.bottom - client.top), scale);

    const int hitIndex = HitTestPendingObject(px, py, scale, imageRect.left, imageRect.top);
    if (hitIndex >= 0)
    {
        PendingTextObject& object = m_pendingObjects[static_cast<size_t>(hitIndex)];
        m_selectedObjectId = object.id;
        m_textDialog.SetCurrentItem(object.item);
        m_draggingObjectId = object.id;
        m_dragUndoStateSaved = false;
        m_dragStartClient.x = static_cast<LONG>(px);
        m_dragStartClient.y = static_cast<LONG>(py);
        m_dragOffsetX = imageX - object.item.x;
        m_dragOffsetY = imageY - object.item.y;
        ::SetCapture(m_hWnd);
        UpdateDialogButtons();
        Invalidate(FALSE);
        return 0;
    }

    // Add a deliberate deselection step: if an object is currently selected,
    // the first click on an empty image area only clears the selection. A
    // subsequent click, while already deselected, creates a new object using
    // the text and properties currently shown in the dialog.
    if (m_selectedObjectId != 0)
    {
        DeselectSelectedObject();
        return 0;
    }

    TextItem item = m_textDialog.GetCurrentItem();
    item.x = imageX;
    item.y = imageY;

    PushUndoState();
    PendingTextObject object;
    object.id = m_nextObjectId++;
    object.item = item;
    m_pendingObjects.push_back(object);
    m_selectedObjectId = object.id;
    m_textDialog.SetCurrentItem(item);
    UpdateDialogButtons();
    Invalidate(FALSE);
    return 0;
}

LRESULT CImageTextFrame::OnMouseMove(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    if (m_draggingObjectId == 0 || (wParam & MK_LBUTTON) == 0)
        return 0;

    const float clientX = static_cast<float>(GET_X_LPARAM(lParam));
    const float clientY = static_cast<float>(GET_Y_LPARAM(lParam));
    if (std::abs(clientX - static_cast<float>(m_dragStartClient.x)) < 2.0f &&
        std::abs(clientY - static_cast<float>(m_dragStartClient.y)) < 2.0f)
        return 0;

    const int index = FindPendingObjectIndex(m_draggingObjectId);
    if (index < 0)
        return 0;

    float imageX = 0.0f, imageY = 0.0f;
    if (!ClientToImagePoint(clientX, clientY, imageX, imageY, false))
        return 0;

    if (!m_dragUndoStateSaved)
    {
        PushUndoState();
        m_dragUndoStateSaved = true;
    }

    PendingTextObject& object = m_pendingObjects[static_cast<size_t>(index)];
    object.item.x = (std::max)(0.0f, (std::min)(imageX - m_dragOffsetX, static_cast<float>(m_imageWidth - 1)));
    object.item.y = (std::max)(0.0f, (std::min)(imageY - m_dragOffsetY, static_cast<float>(m_imageHeight - 1)));
    Invalidate(FALSE);
    return 0;
}

LRESULT CImageTextFrame::OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&)
{
    if (m_draggingObjectId != 0)
    {
        m_draggingObjectId = 0;
        m_dragUndoStateSaved = false;
        if (::GetCapture() == m_hWnd)
            ::ReleaseCapture();
        Invalidate(FALSE);
    }
    return 0;
}

LRESULT CImageTextFrame::OnRButtonUp(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    if (!m_imageBitmap || m_pendingObjects.empty())
        return 0;

    const float px = static_cast<float>(GET_X_LPARAM(lParam));
    const float py = static_cast<float>(GET_Y_LPARAM(lParam));
    RECT client = {};
    GetClientRect(&client);
    float scale = 1.0f;
    const D2D1_RECT_F imageRect = CalculateImageRect(
        static_cast<float>(client.right - client.left),
        static_cast<float>(client.bottom - client.top), scale);
    const int hitIndex = HitTestPendingObject(px, py, scale, imageRect.left, imageRect.top);
    if (hitIndex < 0)
        return 0;

    // Select the object under the pointer before showing its context menu.
    m_selectedObjectId = m_pendingObjects[static_cast<size_t>(hitIndex)].id;
    m_textDialog.SetCurrentItem(m_pendingObjects[static_cast<size_t>(hitIndex)].item);
    UpdateDialogButtons();
    Invalidate(FALSE);

    HMENU contextMenu = ::CreatePopupMenu();
    if (!contextMenu)
        return 0;

    ::AppendMenuW(contextMenu, MF_STRING, ID_OBJECT_COMMIT, L"確定する");
    ::AppendMenuW(contextMenu, MF_STRING, ID_OBJECT_DELETE, L"削除する");
    POINT screenPoint = { static_cast<LONG>(px), static_cast<LONG>(py) };
    ::ClientToScreen(m_hWnd, &screenPoint);
    const UINT command = ::TrackPopupMenu(contextMenu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, 0, m_hWnd, nullptr);
    ::DestroyMenu(contextMenu);

    if (command == ID_OBJECT_COMMIT)
        CommitSelectedText();
    else if (command == ID_OBJECT_DELETE)
        DeleteSelectedObject();
    return 0;
}

LRESULT CImageTextFrame::OnInitMenuPopup(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    HMENU menu = reinterpret_cast<HMENU>(wParam);
    if (menu)
    {
        EnableMenuItem(menu, ID_EDIT_UNDO, MF_BYCOMMAND | (m_undoStack.empty() ? MF_GRAYED : MF_ENABLED));
        EnableMenuItem(menu, ID_EDIT_REDO, MF_BYCOMMAND | (m_redoStack.empty() ? MF_GRAYED : MF_ENABLED));
        EnableMenuItem(menu, ID_FILE_SAVEAS, MF_BYCOMMAND | (!m_wicImage ? MF_GRAYED : MF_ENABLED));
    }
    return 0;
}

LRESULT CImageTextFrame::OnClose(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    bHandled = FALSE;
    return 0;
}

LRESULT CImageTextFrame::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    ::DragAcceptFiles(m_hWnd, FALSE);
    m_textDialog.ForceClose();
    m_imageBitmap.Release();
    m_renderTarget.Release();
    PostQuitMessage(0);
    return 0;
}

LRESULT CImageTextFrame::OnDropFiles(UINT, WPARAM wParam, LPARAM, BOOL&)
{
    HDROP drop = reinterpret_cast<HDROP>(wParam);
    if (!drop)
        return 0;

    std::wstring imagePath;
    const UINT fileCount = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < fileCount; ++i)
    {
        const UINT pathLength = ::DragQueryFileW(drop, i, nullptr, 0);
        if (pathLength == 0)
            continue;

        std::vector<wchar_t> buffer(static_cast<size_t>(pathLength) + 1, L'\0');
        if (::DragQueryFileW(drop, i, buffer.data(), static_cast<UINT>(buffer.size())) == 0)
            continue;

        std::wstring candidate(buffer.data());
        if (IsSupportedImagePath(candidate))
        {
            imagePath = std::move(candidate);
            break;
        }
    }

    ::DragFinish(drop);

    if (imagePath.empty())
    {
        MessageBoxW(L"PNG、JPEG、BMP、GIF、TIFF 画像をドロップしてください。",
                    L"Fuchidori", MB_ICONINFORMATION | MB_OK);
        return 0;
    }

    if (!ConfirmDiscardPendingObjects())
        return 0;
    const HRESULT hr = LoadImageFile(imagePath);
    if (FAILED(hr))
    {
        wchar_t message[512] = {};
        swprintf_s(message, L"ドロップした画像を読み込めませんでした。\n%s\nHRESULT: 0x%08X",
                   imagePath.c_str(), static_cast<unsigned int>(hr));
        MessageBoxW(message, L"画像読み込みエラー", MB_ICONERROR | MB_OK);
    }
    return 0;
}

LRESULT CImageTextFrame::OnFileOpen(WORD, WORD, HWND, BOOL&)
{
    std::wstring path;
    if (ShowOpenDialog(path))
    {
        if (!ConfirmDiscardPendingObjects())
            return 0;
        const HRESULT hr = LoadImageFile(path);
        if (FAILED(hr))
        {
            wchar_t message[512] = {};
            swprintf_s(message, L"画像を読み込めませんでした。\nHRESULT: 0x%08X", static_cast<unsigned int>(hr));
            MessageBoxW(message, L"画像読み込みエラー", MB_ICONERROR | MB_OK);
        }
    }
    return 0;
}

LRESULT CImageTextFrame::OnFileSaveAs(WORD, WORD, HWND, BOOL&)
{
    if (!m_wicImage)
    {
        MessageBoxW(L"先に画像を開いてください。", L"保存", MB_ICONINFORMATION | MB_OK);
        return 0;
    }
    std::wstring path;
    if (ShowSaveDialog(path))
    {
        const HRESULT hr = SaveFlattenedImage(path);
        if (FAILED(hr))
        {
            wchar_t message[512] = {};
            swprintf_s(message, L"画像を保存できませんでした。\nHRESULT: 0x%08X", static_cast<unsigned int>(hr));
            MessageBoxW(message, L"保存エラー", MB_ICONERROR | MB_OK);
        }
    }
    return 0;
}

LRESULT CImageTextFrame::OnFileExit(WORD, WORD, HWND, BOOL&)
{
    PostMessage(WM_CLOSE);
    return 0;
}

LRESULT CImageTextFrame::OnUndo(WORD, WORD, HWND, BOOL&)
{
    Undo();
    return 0;
}

LRESULT CImageTextFrame::OnRedo(WORD, WORD, HWND, BOOL&)
{
    Redo();
    return 0;
}

bool CImageTextFrame::ShowOpenDialog(std::wstring& path)
{
    wchar_t fileName[32768] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"画像ファイル\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0すべてのファイル\0*.*\0\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = static_cast<DWORD>(_countof(fileName));
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle = L"画像を開く";
    if (!GetOpenFileNameW(&ofn))
        return false;
    path = fileName;
    return true;
}

bool CImageTextFrame::ShowSaveDialog(std::wstring& path)
{
    wchar_t fileName[32768] = L"image_with_text.png";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"PNG 画像 (*.png)\0*.png\0JPEG 画像 (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0Bitmap 画像 (*.bmp)\0*.bmp\0\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = static_cast<DWORD>(_countof(fileName));
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    ofn.lpstrDefExt = L"png";
    ofn.lpstrTitle = L"画像を保存";
    if (!GetSaveFileNameW(&ofn))
        return false;
    path = fileName;
    return true;
}

HRESULT CImageTextFrame::LoadImageFile(const std::wstring& path)
{
    if (!m_wicFactory)
        return E_FAIL;

    CComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = m_wicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
        return hr;

    CComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
        return hr;

    UINT width = 0, height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
        return FAILED(hr) ? hr : E_FAIL;

    CComPtr<IWICFormatConverter> converter;
    hr = m_wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr))
        return hr;
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return hr;

    CComPtr<IWICBitmap> wicBitmap;
    hr = m_wicFactory->CreateBitmapFromSource(converter, WICBitmapCacheOnLoad, &wicBitmap);
    if (FAILED(hr))
        return hr;

    CComPtr<ID2D1Bitmap> d2dBitmap;
    hr = EnsureRenderTarget();
    if (FAILED(hr))
        return hr;
    hr = m_renderTarget->CreateBitmapFromWicBitmap(wicBitmap, &d2dBitmap);
    if (FAILED(hr))
        return hr;

    m_imageBitmap = d2dBitmap;
    m_wicImage = wicBitmap;
    m_imageWidth = width;
    m_imageHeight = height;
    m_imagePath = path;
    m_committedTextItems.clear();
    m_pendingObjects.clear();
    m_selectedObjectId = 0;
    m_draggingObjectId = 0;
    m_undoStack.clear();
    m_redoStack.clear();
    m_textDialog.ResetToDefaults(L"");
    UpdateDialogButtons();
    Invalidate(FALSE);
    return S_OK;
}

HRESULT CImageTextFrame::SaveFlattenedImage(const std::wstring& path)
{
    if (!m_wicFactory || !m_d2dFactory || !m_wicImage)
        return E_FAIL;

    const std::wstring ext = LowercaseExtension(path);
    const bool isJpeg = (ext == L".jpg" || ext == L".jpeg");
    const bool isBmp = (ext == L".bmp");
    const bool isPng = !isJpeg && !isBmp;
    const GUID& container = isJpeg ? GUID_ContainerFormatJpeg :
                            (isBmp ? GUID_ContainerFormatBmp : GUID_ContainerFormatPng);

    CComPtr<IWICBitmap> outputBitmap;
    HRESULT hr = m_wicFactory->CreateBitmap(m_imageWidth, m_imageHeight,
        GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &outputBitmap);
    if (FAILED(hr))
        return hr;

    CComPtr<ID2D1RenderTarget> outputTarget;
    hr = m_d2dFactory->CreateWicBitmapRenderTarget(outputBitmap,
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f),
        &outputTarget);
    if (FAILED(hr))
        return hr;

    CComPtr<ID2D1Bitmap> sourceBitmap;
    hr = outputTarget->CreateBitmapFromWicBitmap(m_wicImage, &sourceBitmap);
    if (FAILED(hr))
        return hr;

    outputTarget->BeginDraw();
    if (isPng)
        outputTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    else
        outputTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
    const D2D1_RECT_F fullRect = D2D1::RectF(0.0f, 0.0f,
        static_cast<float>(m_imageWidth), static_cast<float>(m_imageHeight));
    outputTarget->DrawBitmap(sourceBitmap, fullRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    for (const TextItem& item : m_committedTextItems)
        DrawTextItem(outputTarget, item, 1.0f, 0.0f, 0.0f);
    hr = outputTarget->EndDraw();
    if (FAILED(hr))
        return hr;

    CComPtr<IWICStream> stream;
    hr = m_wicFactory->CreateStream(&stream);
    if (FAILED(hr))
        return hr;
    hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (FAILED(hr))
        return hr;

    CComPtr<IWICBitmapEncoder> encoder;
    hr = m_wicFactory->CreateEncoder(container, nullptr, &encoder);
    if (FAILED(hr))
        return hr;
    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr))
        return hr;

    CComPtr<IWICBitmapFrameEncode> frame;
    CComPtr<IPropertyBag2> properties;
    hr = encoder->CreateNewFrame(&frame, &properties);
    if (FAILED(hr))
        return hr;

    if (isJpeg && properties)
    {
        PROPBAG2 option = {};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_R4;
        value.fltVal = 0.95f;
        properties->Write(1, &option, &value);
        VariantClear(&value);
    }

    hr = frame->Initialize(properties);
    if (FAILED(hr))
        return hr;
    hr = frame->SetSize(m_imageWidth, m_imageHeight);
    if (FAILED(hr))
        return hr;

    WICPixelFormatGUID pixelFormat = isPng ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR;
    hr = frame->SetPixelFormat(&pixelFormat);
    if (FAILED(hr))
        return hr;

    CComPtr<IWICFormatConverter> outputConverter;
    hr = m_wicFactory->CreateFormatConverter(&outputConverter);
    if (FAILED(hr))
        return hr;
    hr = outputConverter->Initialize(outputBitmap, pixelFormat, WICBitmapDitherTypeNone,
        nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return hr;

    hr = frame->WriteSource(outputConverter, nullptr);
    if (FAILED(hr))
        return hr;
    hr = frame->Commit();
    if (FAILED(hr))
        return hr;
    return encoder->Commit();
}

void CImageTextFrame::OnTextDialogChanged()
{
    if (!m_textDialog.IsWindow() || m_textDialog.IsUpdatingControls())
        return;

    TextItem edited = m_textDialog.GetCurrentItem();
    const int selectedIndex = FindPendingObjectIndex(m_selectedObjectId);
    if (selectedIndex >= 0)
    {
        PendingTextObject& selected = m_pendingObjects[static_cast<size_t>(selectedIndex)];
        edited.x = selected.item.x;
        edited.y = selected.item.y;
        if (!SameTextItem(selected.item, edited))
        {
            PushUndoState();
            selected.item = edited;
        }
    }
    else
    {
        // With no selected object, the controls define the defaults for future
        // objects. The current typed string is not part of those defaults.
        edited.text.clear();
        edited.x = edited.y = 0.0f;
        m_textDialog.UpdateDefaultsFromCurrentItem(edited);
    }

    UpdateDialogButtons();
    Invalidate(FALSE);
}

void CImageTextFrame::CommitSelectedText()
{
    const int index = FindPendingObjectIndex(m_selectedObjectId);
    if (index < 0)
        return;

    const TextItem item = m_pendingObjects[static_cast<size_t>(index)].item;
    PushUndoState();

    // Confirming an empty object simply removes the placeholder. It is not
    // retained as an invisible editable object.
    if (!item.text.empty())
        m_committedTextItems.push_back(item);
    m_pendingObjects.erase(m_pendingObjects.begin() + index);
    m_selectedObjectId = 0;
    m_draggingObjectId = 0;
    if (::GetCapture() == m_hWnd)
        ::ReleaseCapture();
    m_textDialog.ResetToDefaults(L"");
    UpdateDialogButtons();
    Invalidate(FALSE);
}

void CImageTextFrame::CommitAllPendingTexts()
{
    if (m_pendingObjects.empty())
        return;

    PushUndoState();
    for (const PendingTextObject& object : m_pendingObjects)
    {
        if (!object.item.text.empty())
            m_committedTextItems.push_back(object.item);
        // Empty objects are intentionally discarded during bulk confirmation.
    }

    m_pendingObjects.clear();
    m_selectedObjectId = 0;
    m_draggingObjectId = 0;
    if (::GetCapture() == m_hWnd)
        ::ReleaseCapture();
    m_textDialog.ResetToDefaults(L"");
    UpdateDialogButtons();
    Invalidate(FALSE);
}

void CImageTextFrame::DeselectSelectedObject()
{
    if (m_selectedObjectId == 0)
        return;
    m_selectedObjectId = 0;
    m_draggingObjectId = 0;
    if (::GetCapture() == m_hWnd)
        ::ReleaseCapture();
    m_textDialog.ResetToDefaults(L"");
    UpdateDialogButtons();
    Invalidate(FALSE);
}

void CImageTextFrame::DeleteSelectedObject()
{
    const int index = FindPendingObjectIndex(m_selectedObjectId);
    if (index < 0)
        return;

    PushUndoState();
    m_pendingObjects.erase(m_pendingObjects.begin() + index);
    m_selectedObjectId = 0;
    m_draggingObjectId = 0;
    if (::GetCapture() == m_hWnd)
        ::ReleaseCapture();
    m_textDialog.ResetToDefaults(L"");
    UpdateDialogButtons();
    Invalidate(FALSE);
}

EditorState CImageTextFrame::CaptureState() const
{
    EditorState state;
    state.committedTextItems = m_committedTextItems;
    state.pendingObjects = m_pendingObjects;
    state.selectedObjectId = m_selectedObjectId;
    return state;
}

void CImageTextFrame::PushUndoState()
{
    m_undoStack.push_back(CaptureState());
    if (m_undoStack.size() > 100)
        m_undoStack.erase(m_undoStack.begin());
    m_redoStack.clear();
}

void CImageTextFrame::RestoreState(const EditorState& state)
{
    m_committedTextItems = state.committedTextItems;
    m_pendingObjects = state.pendingObjects;
    m_selectedObjectId = state.selectedObjectId;
    m_draggingObjectId = 0;
    m_dragUndoStateSaved = false;
    if (::GetCapture() == m_hWnd)
        ::ReleaseCapture();
    SyncDialogToSelection();
    UpdateDialogButtons();
    Invalidate(FALSE);
}

void CImageTextFrame::SyncDialogToSelection()
{
    const int index = FindPendingObjectIndex(m_selectedObjectId);
    if (index >= 0)
    {
        m_textDialog.SetCurrentItem(m_pendingObjects[static_cast<size_t>(index)].item);
        return;
    }

    m_selectedObjectId = 0;
    m_textDialog.ResetToDefaults(L"");
}

void CImageTextFrame::Undo()
{
    if (m_undoStack.empty())
        return;
    m_redoStack.push_back(CaptureState());
    EditorState previous = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    RestoreState(previous);
}

void CImageTextFrame::Redo()
{
    if (m_redoStack.empty())
        return;
    m_undoStack.push_back(CaptureState());
    EditorState next = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    RestoreState(next);
}

bool CImageTextFrame::ConfirmDiscardPendingObjects()
{
    if (m_pendingObjects.empty() && m_committedTextItems.empty())
        return true;
    return MessageBoxW(
        L"現在の画像に対する文字編集は、新しい画像を開くと破棄されます。続行しますか？",
        L"Fuchidori", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES;
}

