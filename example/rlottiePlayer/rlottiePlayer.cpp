// rlottiePlayer.cpp : Defines the entry point for the application.
//


#include "framework.h"
#include "rlottiePlayer.h"
using namespace Gdiplus;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                        // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND mainWindow;                                    // Main Window Instance
HWND hTextFileToBeOpened;                               // openDialog file path
HWND hBtnPlay;
HWND hSliderPlay, hSliderCanvasResize;
UINT curFrame = 0;
RlottieBitmap anim;                                          // rendered Animation Bitmap
RECT animRect, backRect;
size_t animWidth, animHeight;
Gdiplus::Color backColor(255, 255, 255, 255);
Gdiplus::Color borderColor(255, 0, 0, 0);
bool isBackgroundChanged = false;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void openJSONFileDialog(HWND);
void initUIControl(HWND);
void dlgUICommand(HWND, WPARAM);
void resizeCanvas(HWND, int);
void changeBackgroundColor(int r, int g, int b);

// Animation Rendering Functions
void draw(HDC);
Bitmap* CreateBitmap(void* data, unsigned int width, unsigned int height);
void renderAnimation(UINT);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // initialize Gdiplus
    Gdiplus::GdiplusStartupInput gdiplusStartUpInput;
    ULONG_PTR gdiplustoken;
    Gdiplus::GdiplusStartup(&gdiplustoken, &gdiplusStartUpInput, nullptr);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_RLOTTIEPLAYER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RLOTTIEPLAYER));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    Gdiplus::GdiplusShutdown(gdiplustoken);
    return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RLOTTIEPLAYER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_RLOTTIEPLAYER);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    mainWindow = CreateWindowEx(0, szWindowClass, szTitle, dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, WND_WIDTH, WND_HEIGHT, nullptr, nullptr, hInstance, nullptr);

    if (!mainWindow)
    {
        return FALSE;
    }

    ShowWindow(mainWindow, nCmdShow);
    UpdateWindow(mainWindow);
    SetMenu(mainWindow, NULL);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool isplay = false;
    int wmId = LOWORD(wParam);

    switch (message)
    {
    case WM_CREATE:
    {
        initUIControl(hWnd);
        break;
    }
    case WM_TIMER:
    {
        switch (wmId)
        {
        case TIMER_PLAY_ANIM:
        {
            renderAnimation(curFrame + 1);
            SendMessage(hSliderPlay, TBM_SETPOS, TRUE, curFrame);
            break;
        }
        default:
            break;
        }
        break;
    }
    case WM_COMMAND:
    {
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        case BTN_BROWSE:
            openJSONFileDialog(hWnd);
            break;

        case BTN_PLAY:
        {
            LPWSTR textBtnPlay;
            USES_CONVERSION;
            if (isplay)
            {
                isplay = false;
                textBtnPlay = A2W("Play");
                KillTimer(hWnd, TIMER_PLAY_ANIM);
            }
            else
            {
                isplay = true;
                textBtnPlay = A2W("Pause");
                SetTimer(hWnd, TIMER_PLAY_ANIM, 10, NULL);
            }
            SetWindowText(hBtnPlay, textBtnPlay);
            break;
        }

        case WM_DROPFILES:
            break;
        case BTN_WHITE:
            changeBackgroundColor(1, 1, 1);
            break;
        case BTN_BLACK:
            changeBackgroundColor(0, 0, 0);
            break;
        case BTN_RED:
            changeBackgroundColor(1, 0, 0);
            break;
        case BTN_GREEN:
            changeBackgroundColor(0, 1, 0);
            break;
        case BTN_BLUE:
            changeBackgroundColor(0, 0, 1);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_HSCROLL:
    {
        if ((lParam != 0) && (reinterpret_cast<HWND>(lParam) == hSliderPlay))
        {
            UINT frameNum = SendDlgItemMessage(hWnd, SLIDER_PLAY, TBM_GETPOS, 0, 0);
            renderAnimation(frameNum);
        }
        else if ((lParam != 0) && (reinterpret_cast<HWND>(lParam) == hSliderCanvasResize))
        {
            static int curSize = anim.width / RESIZE_LENGTH;
            int newSize = SendDlgItemMessage(hWnd, SLIDER_CANVAS_RESIZE, TBM_GETPOS, 0, 0);
            resizeCanvas(hWnd, (curSize - newSize) * RESIZE_LENGTH);
            curSize = newSize;
        }
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        draw(hdc);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_CTLCOLORSTATIC:
    {
        static HBRUSH hBrushEdit = CreateSolidBrush(RGB(255, 255, 255));
        return (LRESULT)hBrushEdit;
    }

    case WM_DESTROY:
        freeAnimation();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void openJSONFileDialog(HWND hDlg)
{
    OPENFILENAME ofn;       // common dialog box structure
    TCHAR szFile[260] = { 0 };       // if using TCHAR macros

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = _T("JSON\0*.json\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
    {
        SetWindowText(hTextFileToBeOpened, ofn.lpstrFile);
        // LPWSTR(w_char*) -> LPSTR(char*)
        USES_CONVERSION;
        LPSTR path = W2A(ofn.lpstrFile);

        setAnimation(path, &animWidth, &animHeight);
        // init play slider
        SendMessage(hSliderPlay, TBM_SETRANGE, FALSE, MAKELPARAM(0, getTotalFrame()));
        SendMessage(hSliderPlay, TBM_SETPOS, TRUE, 0);
        renderAnimation(0);
    }
}

void draw(HDC hdc)
{
    Graphics gf(hdc);
    int half_interval = UI_INTERVAL / 2;

    // background
    SolidBrush brush(backColor);
    int back_y = half_interval + BTN_HEIGHT;
    int back_height = back_y + BMP_MAX_LEN + UI_INTERVAL;
    if (isBackgroundChanged)
    {
        isBackgroundChanged = false;
        gf.FillRectangle(&brush, 0, back_y, WND_WIDTH, back_height);
    }

    // image borderline
    Pen pen(borderColor);
    gf.DrawRectangle(&pen, anim.x - half_interval, anim.y - half_interval, anim.width + half_interval * 2, anim.height + half_interval * 2);

    // image
    if (anim.image != NULL)
    {
        gf.DrawImage(anim.image, anim.x, anim.y, anim.width, anim.height);
    }
}

Bitmap* CreateBitmap(void* data, unsigned int width, unsigned int height)
{
    BITMAPINFO Info;
    memset(&Info, 0, sizeof(Info));

    Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    Info.bmiHeader.biWidth = width;
    Info.bmiHeader.biHeight = height;
    Info.bmiHeader.biPlanes = 1;
    Info.bmiHeader.biBitCount = 32;
    Info.bmiHeader.biCompression = BI_RGB;
    Info.bmiHeader.biSizeImage = 0;  //(((32 * width + 31) & ~31) / 8) * height;

    return new Gdiplus::Bitmap(&Info, data);
}

void renderAnimation(UINT frameNum)
{
    if (isAnimNULL()) return;
    if (anim.image != NULL) delete anim.image;

    curFrame = frameNum % getTotalFrame();

    // render
    UINT* resRender = renderRLottieAnimation(curFrame);
    anim.image = CreateBitmap(resRender, animWidth, animHeight);
    anim.image->RotateFlip(RotateNoneFlipY);
    // call WM_PAINT message
    InvalidateRect(mainWindow, &animRect, FALSE);
}

void initUIControl(HWND hWnd)
{
    int half_ui_interval = UI_INTERVAL / 2;

    // button browse
    int browse_x = UI_INTERVAL;
    int browse_y = half_ui_interval;
    CreateWindow(L"button", L"Browse", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        browse_x, browse_y, BTN_WIDTH, BTN_HEIGHT, hWnd, (HMENU)BTN_BROWSE, hInst, NULL);

    // textbox FilePath
    int textFile_x = browse_x + BTN_WIDTH + UI_INTERVAL;
    int textFile_y = browse_y;
    hTextFileToBeOpened = CreateWindowEx(0, L"static", L"No file selected.", WS_CHILD | WS_VISIBLE | ES_LEFT,
        textFile_x, textFile_y, WND_WIDTH * 0.6, TEXT_HEIGHT, hWnd, (HMENU)TEXT_FILENAME, hInst, 0);

    // image
    anim.x = WND_WIDTH / 4;
    anim.y = browse_y + BTN_HEIGHT + UI_INTERVAL * 2;
    anim.width = BMP_MAX_LEN;
    anim.height = anim.width;

    // animating range
    SetRect(&animRect,
        anim.x - UI_INTERVAL,
        anim.y - UI_INTERVAL,
        anim.x + anim.width + UI_INTERVAL * 2,
        anim.y + anim.height + UI_INTERVAL * 2
    );

    // background range
    SetRect(&backRect,
        0,
        anim.y - UI_INTERVAL,
        WND_WIDTH,
        anim.y + anim.height + UI_INTERVAL * 2);

    // text Background Color
    int textBC_x = WND_WIDTH / 20;
    int textBC_y = anim.y + anim.height + UI_INTERVAL * 2;
    CreateWindowEx(0, L"static", L"Background Color", WS_CHILD | WS_VISIBLE | ES_LEFT,
        textBC_x, textBC_y, 120, TEXT_HEIGHT, hWnd, (HMENU)TEXT_FILENAME, hInst, 0);

    // radio button
    // white
    int white_x = WND_WIDTH / 20;
    int white_y = textBC_y + TEXT_HEIGHT + half_ui_interval;
    CreateWindowEx(0, L"button", L"White", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        white_x, white_y, RDOBTN_WIDTH, RDOBTN_HEIGHT, hWnd, (HMENU)BTN_WHITE, hInst, NULL);

    // black
    int black_x = white_x + RDOBTN_WIDTH + half_ui_interval;
    int black_y = white_y;
    CreateWindowEx(0, L"button", L"Black", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        black_x, black_y, RDOBTN_WIDTH, RDOBTN_HEIGHT, hWnd, (HMENU)BTN_BLACK, hInst, NULL);

    // red
    int red_x = black_x + RDOBTN_WIDTH + half_ui_interval;
    int red_y = white_y;
    CreateWindowEx(0, L"button", L"Red", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        red_x, red_y, RDOBTN_WIDTH, RDOBTN_HEIGHT, hWnd, (HMENU)BTN_RED, hInst, NULL);

    // green
    int green_x = red_x + RDOBTN_WIDTH + half_ui_interval;
    int green_y = white_y;
    CreateWindowEx(0, L"button", L"Green", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        green_x, green_y, RDOBTN_WIDTH, RDOBTN_HEIGHT, hWnd, (HMENU)BTN_GREEN, hInst, NULL);

    // blue
    int blue_x = green_x + RDOBTN_WIDTH + half_ui_interval;
    int blue_y = white_y;
    CreateWindowEx(0, L"button", L"Blue", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        blue_x, blue_y, RDOBTN_WIDTH, RDOBTN_HEIGHT, hWnd, (HMENU)BTN_BLUE, hInst, NULL);

    CheckRadioButton(hWnd, BTN_WHITE, BTN_BLUE, BTN_WHITE);

    // text Canvas Resize
    int textCR_x = WND_WIDTH / 2;
    int textCR_y = textBC_y;
    CreateWindowEx(0, L"static", L"Canvas Resize", WS_CHILD | WS_VISIBLE | ES_LEFT,
        textCR_x, textCR_y, 120, TEXT_HEIGHT, hWnd, (HMENU)TEXT_FILENAME, hInst, 0);

    // slider Canvas Resize
    int sliderCR_x = textCR_x;
    int sliderCR_y = textCR_y + TEXT_HEIGHT + half_ui_interval;
    hSliderCanvasResize = CreateWindowExW(0, TRACKBAR_CLASSW, NULL, WS_CHILD | WS_VISIBLE | TBS_FIXEDLENGTH | TBS_NOTICKS,
        sliderCR_x, sliderCR_y, WND_WIDTH * 0.2, SLIDER_HEIGHT, hWnd, (HMENU)SLIDER_CANVAS_RESIZE, hInst, NULL);

    // init resize slider
    UINT sizeSlider = anim.width / RESIZE_LENGTH;
    SendMessage(hSliderCanvasResize, TBM_SETRANGE, FALSE, MAKELPARAM(0, sizeSlider));
    SendMessage(hSliderCanvasResize, TBM_SETPOS, TRUE, sizeSlider);

    // button play
    int btnPlay_x = WND_WIDTH / 10;
    int btnPlay_y = red_y + RDOBTN_HEIGHT + UI_INTERVAL * 2;
    hBtnPlay = CreateWindow(L"button", L"Play", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnPlay_x, btnPlay_y, BTN_WIDTH, BTN_HEIGHT, hWnd, (HMENU)BTN_PLAY, hInst, NULL);

    // slider play
    int sliderPlay_x = btnPlay_x + BTN_WIDTH + UI_INTERVAL;
    int sliderPlay_y = btnPlay_y;
    hSliderPlay = CreateWindowExW(0, TRACKBAR_CLASSW, NULL, WS_CHILD | WS_VISIBLE | TBS_FIXEDLENGTH | TBS_NOTICKS,
        sliderPlay_x, sliderPlay_y, WND_WIDTH * 0.6, SLIDER_HEIGHT, hWnd, (HMENU)SLIDER_PLAY, hInst, NULL);
}

void resizeCanvas(HWND hWnd, int resizeValue)
{
    isBackgroundChanged = true;
    anim.x += resizeValue / 2;
    anim.y += resizeValue / 2;
    anim.width -= resizeValue;
    anim.height -= resizeValue;
    InvalidateRect(hWnd, &animRect, TRUE);
}

void changeBackgroundColor(int r, int g, int b)
{
    isBackgroundChanged = true;
    backColor = Gdiplus::Color(r * 255, g * 255, b * 255);
    if (r + g + b == 0) borderColor = Gdiplus::Color(255, 255, 255);
    else borderColor = Gdiplus::Color(0, 0, 0);
    setAnimationColor(r, g, b);
    renderAnimation(curFrame);
    InvalidateRect(mainWindow, &backRect, FALSE);
}