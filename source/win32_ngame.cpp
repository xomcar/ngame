#include <windows.h>

#define local_persist static
#define global_variable static
#define internal static

// TODO global for now
global_variable bool Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void* BitmapMemory;
global_variable HBITMAP BitmapHandle;
global_variable HDC BitmapDeviceContext;

internal void
// DIB = Device Indipendent Bitmap
Win32ResizeDIBSection(int Width, int Height)
{
    // TODO Bulletproof this: dont free first, free after

    if (BitmapHandle) {
        DeleteObject(BitmapHandle);
    }

    if (!BitmapDeviceContext) {
        // TODO: should recreate in some circumstances
        BitmapDeviceContext = CreateCompatibleDC(0);
    }

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = Width;
    BitmapInfo.bmiHeader.biHeight = Height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    BitmapHandle = CreateDIBSection(
        BitmapDeviceContext,
        &BitmapInfo,
        DIB_RGB_COLORS,
        &BitmapMemory,
        0,
        0);
}

internal void
Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height)
{
    StretchDIBits(DeviceContext,
        X, Y, Width, Height,
        X, Y, Width, Height,
        BitmapMemory,
        &BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
    UINT Message,
    WPARAM wParameter,
    LPARAM lParameter)
{
    LRESULT Result = 0;

    switch (Message) {
    case WM_SIZE: {
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        int Width = ClientRect.right - ClientRect.left;
        int Height = ClientRect.bottom - ClientRect.top;
        Win32ResizeDIBSection(Width, Height);
    } break;

    case WM_DESTROY: {
        // TODO: handle this as and error, recreate window?
        Running = false;
    } break;

    case WM_CLOSE: {
        // TODO: handle with message to the user?
        Running = false;
    } break;

    case WM_ACTIVATEAPP: {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        int X = Paint.rcPaint.left;
        int Y = Paint.rcPaint.top;
        int Width = Paint.rcPaint.right - Paint.rcPaint.left;
        int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
        local_persist DWORD Operation = BLACKNESS;
        PatBlt(DeviceContext, X, Y, Width, Height, Operation);
        if (Operation == WHITENESS) {
            Operation = BLACKNESS;
        } else {
            Operation = WHITENESS;
        }
        EndPaint(Window, &Paint);
    } break;

    default: {
        // OutputDebugStringA("WM_UNHANDLED_DEFAULT\n");
        Result = DefWindowProc(Window, Message, wParameter, lParameter);
    } break;
    }
    return (Result);
}

int CALLBACK
WinMain(HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommandLine,
    int ShowCode)
{
    // Populate window class
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon = TODO;
    WindowClass.lpszClassName = "NGameWindowClass";

    // Register window class, enable message handling
    if (RegisterClassA(&WindowClass)) {
        HWND WindowHandle = CreateWindowExA(0,
            WindowClass.lpszClassName,
            "NGame",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
        if (WindowHandle) {
            MSG Message;
            Running = true;
            while (Running) {
                BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
                if (MessageResult > 0) {
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                } else {
                    break;
                }
            }
        } else {
            // TODO: log error
        }
    } else {
        // TODO: log error
    }
    return (0);
}
