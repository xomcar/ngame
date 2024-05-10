#include <windows.h>
#include "types.h"
#include <dwmapi.h>

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void*      Memory;
    int        Width;
    int        Height;
    int        Pitch;
    int        BytesPerPixel;
};

#define BUFFER_WIDTH 1280
#define BUFFER_HEIGHT 720
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable bool                   GlobalRunning;
global_variable BOOL                   GlobalEnableDarkMode = 1;

struct win32_window_dimension
{
    int Width;
    int Height;
};

internal win32_window_dimension
GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT WindowRect;
    GetClientRect(Window, &WindowRect);
    Result.Width  = WindowRect.right - WindowRect.left;
    Result.Height = WindowRect.bottom - WindowRect.top;
    return Result;
}

internal void
RenderGradient(win32_offscreen_buffer Buffer, int XOffSet, int YOffset)
{
    u8* Row = (u8*) Buffer.Memory;
    for (int Y = 0; Y < Buffer.Height; ++Y)
    {
        u32* Pixel = (u32*) Row;
        for (int X = 0; X < Buffer.Width; ++X)
        {
            u8 Blue  = (X + XOffSet);
            u8 Green = (Y + YOffset);
            *Pixel   = ((Green << 8) | Blue);
            Pixel++;
        }
        Row += Buffer.Pitch;
    }
}

internal void
Win32PopulateBuffer(win32_offscreen_buffer* Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Height        = Height;
    Buffer->Width         = Width;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize        = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth       = Width;
    Buffer->Info.bmiHeader.biHeight      = -Height;  // negative to have top down y direction
    Buffer->Info.bmiHeader.biPlanes      = 1;
    Buffer->Info.bmiHeader.biBitCount    = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Width * Height) * Buffer->BytesPerPixel;
    Buffer->Memory       = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch        = Width * Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(HDC                    DeviceContext,
                           int                    WindowWidth,
                           int                    WindowHeight,
                           win32_offscreen_buffer Buffer)
{
    // TODO: correct aspect ratio
    StretchDIBits(DeviceContext,
                  // X, Y, Width, Height,
                  // X, Y, Width, Height,
                  0,
                  0,
                  WindowWidth,
                  WindowHeight,
                  0,
                  0,
                  Buffer.Width,
                  Buffer.Height,
                  Buffer.Memory,
                  &Buffer.Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM wParameter, LPARAM lParameter)
{
    LRESULT Result = 0;

    switch (Message)
    {
        case WM_DESTROY:
        {
            // TODO: handle this as and error, recreate window?
            GlobalRunning = false;
        }
        break;

        case WM_CLOSE:
        {
            // TODO: handle with message to the user?
            GlobalRunning = false;
        }
        break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        }
        break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC         DeviceContext = BeginPaint(Window, &Paint);

            win32_window_dimension Dimension = GetWindowDimension(Window);
            Win32DisplayBufferInWindow(
                DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer);
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

    Win32PopulateBuffer(&GlobalBackBuffer, BUFFER_WIDTH, BUFFER_HEIGHT);

    WindowClass.style       = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance   = Instance;
    // TODO: WindowClass.hIcon;
    WindowClass.lpszClassName = "NGameWindowClass";

    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(0,
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

        HRESULT ret = DwmSetWindowAttribute(Window,
                                            DWMWA_USE_IMMERSIVE_DARK_MODE,
                                            &GlobalEnableDarkMode,
                                            sizeof(GlobalEnableDarkMode));
        if (Window)
        {
            HDC DeviceContext = GetDC(Window);
            int xOffset       = 0;
            int yOffset       = 0;

            GlobalRunning = true;

            // Main rendering loop
            while (GlobalRunning)
            {
                MSG Message;
                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                RenderGradient(GlobalBackBuffer, xOffset, yOffset);

                win32_window_dimension Dimension = GetWindowDimension(Window);
                Win32DisplayBufferInWindow(
                    DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer);
                ++xOffset;
                yOffset += 2;
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
