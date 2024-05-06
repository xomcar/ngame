#include <windows.h>

#define local_persist static
#define global_variable static
#define internal static

// TODO global for now
global_variable bool Running;

internal void
ResizeDIBSection()
{
}

LRESULT CALLBACK
MainWindowCallback(HWND Window,
    UINT Message,
    WPARAM wParameter,
    LPARAM lParameter)
{
    LRESULT Result = 0;

    switch (Message) {
    case WM_SIZE: {
        ResizeDIBSection();
        OutputDebugStringA("WM_SIZE\n");
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
    WindowClass.lpfnWndProc = MainWindowCallback;
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
