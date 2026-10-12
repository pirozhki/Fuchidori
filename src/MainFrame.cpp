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
}

BOOL CImageTextFrame::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg && pMsg->message == WM_KEYDOWN)
    {
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
        DrawScene(m_renderTarget, true);
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

void CImageTextFrame::DrawScene(ID2D1RenderTarget* target, bool includePreview)
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
            const wchar_t* message = L"[ファイル] → [画像を開く] から画像を読み込んでください。\n画像上をクリックすると、縁取り文字を配置できます。";
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

    for (const TextItem& item : m_textItems)
        DrawTextItem(target, item, scale, imageRect.left, imageRect.top);

    if (includePreview && m_textDialog.IsWindow())
    {
        TextItem preview = m_textDialog.GetCurrentItem();
        if (!preview.text.empty())
            DrawTextItem(target, preview, scale, imageRect.left, imageRect.top);
    }
}

void CImageTextFrame::DrawTextItem(ID2D1RenderTarget* target, const TextItem& item,
                                   float displayScale, float offsetX, float offsetY)
{
    if (!target || item.text.empty() || !m_dwriteFactory)
        return;

    const float fontSize = (std::max)(1.0f, item.fontSizePt * (96.0f / 72.0f) * displayScale);
    const DWRITE_FONT_WEIGHT weight = item.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    const DWRITE_FONT_STYLE style = item.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

    CComPtr<IDWriteTextFormat> format;
    if (FAILED(m_dwriteFactory->CreateTextFormat(
        item.fontName.empty() ? L"Segoe UI" : item.fontName.c_str(), nullptr,
        weight, style, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"ja-JP", &format)))
        return;
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    if (item.vertical)
    {
        // DirectWrite's vertical layout places columns from right to left and
        // characters from top to bottom (including vertical glyph orientation).
        if (FAILED(format->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM)) ||
            FAILED(format->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT)))
            return;
    }

    // For vertical writing, the rightmost column is the first column.
    // Size the layout to the number of explicit lines and shift its origin so
    // the first column remains anchored at the clicked image position.
    float layoutWidth = 100000.0f;
    float layoutX = offsetX + item.x * displayScale;
    if (item.vertical)
    {
        size_t columnCount = 1;
        for (size_t i = 0; i < item.text.size(); ++i)
        {
            if (item.text[i] == L'\n' ||
                (item.text[i] == L'\r' && (i + 1 == item.text.size() || item.text[i + 1] != L'\n')))
                ++columnCount;
        }
        layoutWidth = fontSize * static_cast<float>(columnCount);
        layoutX -= fontSize * static_cast<float>(columnCount - 1);
    }

    CComPtr<IDWriteTextLayout> layout;
    const HRESULT layoutHr = m_dwriteFactory->CreateTextLayout(
        item.text.c_str(), static_cast<UINT32>(item.text.size()), format,
        layoutWidth, 100000.0f, &layout);
    if (FAILED(layoutHr))
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

    const float x = layoutX;
    const float y = offsetY + item.y * displayScale;

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
            const float dx = std::cos(angle) * radius;
            const float dy = std::sin(angle) * radius;
            target->DrawTextLayout(D2D1::Point2F(x + dx, y + dy), layout, outlineBrush);
        }
    }

    target->DrawTextLayout(D2D1::Point2F(x, y), layout, fillBrush);
}

LRESULT CImageTextFrame::OnLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    if (!m_imageBitmap || m_imageWidth == 0 || m_imageHeight == 0)
        return 0;

    RECT client = {};
    GetClientRect(&client);
    float scale = 1.0f;
    const D2D1_RECT_F imageRect = CalculateImageRect(
        static_cast<float>(client.right - client.left),
        static_cast<float>(client.bottom - client.top), scale);
    const float px = static_cast<float>(GET_X_LPARAM(lParam));
    const float py = static_cast<float>(GET_Y_LPARAM(lParam));
    if (scale <= 0.0f || px < imageRect.left || px >= imageRect.right ||
        py < imageRect.top || py >= imageRect.bottom)
        return 0;

    m_previewX = (px - imageRect.left) / scale;
    m_previewY = (py - imageRect.top) / scale;
    if (m_textDialog.IsWindow())
    {
        m_textDialog.SetPosition(m_previewX, m_previewY);
    }
    else
    {
        m_textDialog.SetPosition(m_previewX, m_previewY);
        HWND dialog = m_textDialog.Create(m_hWnd);
        if (!dialog)
        {
            MessageBoxW(L"文字入力ダイアログを作成できませんでした。", L"エラー", MB_ICONERROR | MB_OK);
            return 0;
        }
        m_textDialog.ShowWindow(SW_SHOWNORMAL);
        ::SetFocus(::GetDlgItem(dialog, IDC_TEXT_INPUT));
    }

    Invalidate(FALSE);
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
    CloseEditorWithoutCommit();
    bHandled = FALSE;
    return 0;
}

LRESULT CImageTextFrame::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    ::DragAcceptFiles(m_hWnd, FALSE);
    CloseEditorWithoutCommit();
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

    // Match the behavior of File > Open: discard the current uncommitted preview.
    CloseEditorWithoutCommit();
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
        CloseEditorWithoutCommit();
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
    m_textItems.clear();
    m_undoStack.clear();
    m_redoStack.clear();
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
    for (const TextItem& item : m_textItems)
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
    Invalidate(FALSE);
}

void CImageTextFrame::OnTextDialogClosed(CTextDialog*)
{
    Invalidate(FALSE);
}

void CImageTextFrame::OnTextAccepted(const TextItem& item, float, float)
{
    CommitText(item);
}

void CImageTextFrame::CommitText(const TextItem& item)
{
    if (item.text.empty())
        return;
    m_undoStack.push_back(m_textItems);
    m_redoStack.clear();
    m_textItems.push_back(item);
    Invalidate(FALSE);
}

void CImageTextFrame::Undo()
{
    if (m_undoStack.empty())
        return;
    m_redoStack.push_back(m_textItems);
    m_textItems = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    Invalidate(FALSE);
}

void CImageTextFrame::Redo()
{
    if (m_redoStack.empty())
        return;
    m_undoStack.push_back(m_textItems);
    m_textItems = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    Invalidate(FALSE);
}

void CImageTextFrame::CloseEditorWithoutCommit()
{
    if (m_textDialog.IsWindow())
        m_textDialog.ForceClose();
}
