#include <stdint.h>
#include <windows.h>

#define local_persist static
#define global_variable static
#define internal static

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// TODO: global for now
global_variable bool       Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void*      BitmapMemory;
global_variable int        BitmapWidth;
global_variable int        BitmapHeight;
global_variable int        BytesPerPixel = 4;

internal void
RenderGradient(int XOffSet, int YOffset)
{
    int Width = BitmapWidth;
    int Height = BitmapHeight;
    int Pitch = Width * BytesPerPixel;
    u8* Row = (u8*) BitmapMemory;
    for (int Y = 0; Y < BitmapHeight; ++Y)
    {
        u32* Pixel = (u32*) Row;
        for (int X = 0; X < BitmapWidth; ++X)
        {
            u8 Blue = (X + XOffSet);
            u8 Green = (Y + YOffset);
            *Pixel = ((Green << 8) | Blue);
            Pixel++;
        }
        Row += Pitch;
    }
}

internal void
Win32ResizeDIBSection(int Width, int Height)
{
    if (BitmapMemory)
    {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapHeight = Height;
    BitmapWidth = Width;
    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = Width;
    BitmapInfo.bmiHeader.biHeight = -Height;  // negative to have top down y direction
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Width * Height) * BytesPerPixel;
    BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32UpdateWindow(HDC DeviceContext, RECT* WindowRect, int X, int Y, int Width, int Height)
{
    int WindowWidth = WindowRect->right - WindowRect->left;
    int WindowHeight = WindowRect->bottom - WindowRect->top;
    StretchDIBits(DeviceContext,
                  // X, Y, Width, Height,
                  // X, Y, Width, Height,
                  0,
                  0,
                  BitmapWidth,
                  BitmapHeight,
                  0,
                  0,
                  WindowWidth,
                  WindowHeight,
                  BitmapMemory,
                  &BitmapInfo,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM wParameter, LPARAM lParameter)
{
    LRESULT Result = 0;

    switch (Message)
    {
        case WM_SIZE:
        {
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int NewWidth = ClientRect.right - ClientRect.left;
            int NewHeight = ClientRect.bottom - ClientRect.top;
            Win32ResizeDIBSection(NewWidth, NewHeight);
        }
        break;

        case WM_DESTROY:
        {
            // TODO: handle this as and error, recreate window?
            Running = false;
        }
        break;

        case WM_CLOSE:
        {
            // TODO: handle with message to the user?
            Running = false;
        }
        break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        }
        break;

        case WM_PAINT:
        {
            PAINTSTRUCT         Paint;
            HDC                 DeviceContext = BeginPaint(Window, &Paint);
            int                 X = Paint.rcPaint.left;
            int                 Y = Paint.rcPaint.top;
            int                 Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int                 Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            local_persist DWORD Operation = BLACKNESS;
            PatBlt(DeviceContext, X, Y, Width, Height, Operation);
            if (Operation == WHITENESS)
            {
                Operation = BLACKNESS;
            }
            else
            {
                Operation = WHITENESS;
            }
            EndPaint(Window, &Paint);
        }
        break;

        default:
        {
            // OutputDebugStringA("WM_UNHANDLED_DEFAULT\n");
            Result = DefWindowProc(Window, Message, wParameter, lParameter);
        }
        break;
    }
    return (Result);
}

int APIENTRY
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // TODO: WindowClass.hIcon;
    WindowClass.lpszClassName = "NGameWindowClass";

    if (RegisterClassA(&WindowClass))
    {
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
        if (WindowHandle)
        {
            MSG Message;
            Running = true;
            int xOffset = 0;
            int yOffset = 0;
            // Main rendering loop
            while (Running)
            {
                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        Running = false;
                    }
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }
                HDC  DeviceContext = GetDC(WindowHandle);
                RECT WindowRect;
                GetClientRect(WindowHandle, &WindowRect);
                int WindowWidth = WindowRect.right - WindowRect.left;
                int WindowHeight = WindowRect.bottom - WindowRect.top;
                RenderGradient(xOffset, yOffset);
                Win32UpdateWindow(DeviceContext, &WindowRect, 0, 0, WindowWidth, WindowHeight);
                ReleaseDC(WindowHandle, DeviceContext);
                xOffset++;
            }
        }
        else
        {
            // TODO: log error
        }
    }
    else
    {
        // TODO: log error
    }
    return (0);
}
