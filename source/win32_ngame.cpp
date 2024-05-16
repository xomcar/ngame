#include <windows.h>
#include <dwmapi.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>

#include "types.h"
#include "ngame.cpp"

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
global_variable LPDIRECTSOUNDBUFFER    GlobalSoundBuffer;

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    int SamplesPerSecond;
    int ToneVolume;
    int ToneHz;
    u32 RunningSampleIndex;
    int SineWavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;
};

/* the following allow to create a stub for function in the case the library could not be loaded */
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dxUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return (0); }
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dxUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return (0); }
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary)
    {
        HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state*) GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state*) GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

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

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID Device, LPDIRECTSOUND* pSound, LPUNKNOWN other);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32InitSound(HWND Window, i32 SamplesPerSecond, i32 BufferSize)
{
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary)
    {
        direct_sound_create* DirectSoundCreate
            = (direct_sound_create*) GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat    = {};
            WaveFormat.wFormatTag      = WAVE_FORMAT_PCM;
            WaveFormat.nChannels       = 2;
            WaveFormat.nSamplesPerSec  = SamplesPerSecond;
            WaveFormat.wBitsPerSample  = 16;
            WaveFormat.nBlockAlign     = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize          = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize       = sizeof(BufferDescription);
                BufferDescription.dwFlags      = DSBCAPS_PRIMARYBUFFER;
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    OutputDebugStringA("Primary audio buffer created\n");
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        OutputDebugStringA("Primary audio buffer format set\n");
                    }
                    else
                    {
                        // TODO: diagnostic
                    };
                }
            }
            else
            {
                // TODO: diagnostic
            }
            // create a secondary buffer to write to
            DSBUFFERDESC BufferDescription  = {};
            BufferDescription.dwSize        = sizeof(BufferDescription);
            BufferDescription.dwFlags       = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat   = &WaveFormat;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSoundBuffer, 0)))
            {
                // start playing audio!
                OutputDebugStringA("Secondary audio buffer created\n");
            }
        }
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
    VOID* Region1;
    DWORD Region1Size;
    VOID* Region2;
    DWORD Region2Size;
    if (SUCCEEDED(
            GlobalSoundBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
    {
        // TODO: assert region1/region2 size are valid (must be multiple of 32 bit due to 2ch * 16bit)
        i16*  SampleOut          = (i16*) Region1;
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            f32 t           = (f32) 2.0f * Pi32 * SoundOutput->RunningSampleIndex / (f32) SoundOutput->SineWavePeriod;
            f32 SineValue   = sinf(t);
            i16 SampleValue = (i16) (SineValue * SoundOutput->ToneVolume);
            *SampleOut++    = SampleValue;
            *SampleOut++    = SampleValue;
            ++SoundOutput->RunningSampleIndex;
        }

        SampleOut                = (i16*) Region2;
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
        {
            f32 t           = (f32) 2.0f * Pi32 * SoundOutput->RunningSampleIndex / (f32) SoundOutput->SineWavePeriod;
            f32 SineValue   = sinf(t);
            i16 SampleValue = (i16) (SineValue * SoundOutput->ToneVolume);
            *SampleOut++    = SampleValue;
            *SampleOut++    = SampleValue;
            ++SoundOutput->RunningSampleIndex;
        }
        GlobalSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32InitVideo(win32_offscreen_buffer* Buffer, int Width, int Height)
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
Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, win32_offscreen_buffer* Buffer)
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
                  Buffer->Width,
                  Buffer->Height,
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal LRESULT CALLBACK
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

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            u32  VKCode  = wParameter;
            bool WasDown = ((lParameter & (1 << 30)) != 0);  // check if previously pressed
            bool IsDown  = ((lParameter & (1 << 31)) == 0);  // check if pressed
            if (WasDown != IsDown)                           // skip input if key is pressed continously
            {
                if (VKCode == 'W') {}
                else if (VKCode == 'A') {}
                else if (VKCode == 'S') {}
                else if (VKCode == 'D') {}
                else if (VKCode == 'Q') {}
                else if (VKCode == 'E') {}
                else if (VKCode == VK_ESCAPE)
                {
                    if (IsDown)
                    {
                        OutputDebugStringA("Pressed ESC\n");
                    }
                    if (WasDown)
                    {
                        OutputDebugStringA("Unpressed ESC\n");
                    }
                }
                else if (VKCode == VK_SPACE) {}
            }

            bool32 AltKeyWasDown = (lParameter & (1 << 29));
            if ((VKCode == VK_F4) && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
        }
        break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC         DeviceContext = BeginPaint(Window, &Paint);

            win32_window_dimension Dimension = GetWindowDimension(Window);
            Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);
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
    Win32LoadXInput();
    Win32InitVideo(&GlobalBackBuffer, BUFFER_WIDTH, BUFFER_HEIGHT);

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

        HRESULT ret = DwmSetWindowAttribute(
            Window, DWMWA_USE_IMMERSIVE_DARK_MODE, &GlobalEnableDarkMode, sizeof(GlobalEnableDarkMode));
        if (Window)
        {
            HDC DeviceContext = GetDC(Window);

            int xOffset = 0;
            int yOffset = 0;

            win32_sound_output SoundOutput  = {};
            SoundOutput.SamplesPerSecond    = 48000;
            SoundOutput.ToneHz              = 256;
            SoundOutput.ToneVolume          = 1500;
            SoundOutput.RunningSampleIndex  = 0;
            SoundOutput.SineWavePeriod      = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            SoundOutput.BytesPerSample      = sizeof(i16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            Win32InitSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
            GlobalSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
            GlobalRunning = true;

            LARGE_INTEGER StartTime, EndTime;
            LARGE_INTEGER TicksFrequency;

            QueryPerformanceFrequency(&TicksFrequency);
            // Main rendering loop
            while (GlobalRunning)
            {
                QueryPerformanceCounter(&StartTime);
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

                // TODO: GetState is bugged and hangs for milliseconds if no controller is
                // plugged
                for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // controller available
                        // TODO investigate dwPacketNumber
                        XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                        BOOL Up    = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        BOOL Down  = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        BOOL Left  = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        BOOL Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        BOOL Start = Pad->wButtons & XINPUT_GAMEPAD_START;
                        BOOL Back  = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                        BOOL A     = Pad->wButtons & XINPUT_GAMEPAD_A;
                        BOOL B     = Pad->wButtons & XINPUT_GAMEPAD_B;
                        BOOL X     = Pad->wButtons & XINPUT_GAMEPAD_X;
                        BOOL Y     = Pad->wButtons & XINPUT_GAMEPAD_Y;

                        i16 stickX = Pad->sThumbLX;
                        i16 stickY = Pad->sThumbLY;

                        XINPUT_VIBRATION Vibration = {};
                        if (A)
                        {
                            yOffset += 2;
                            Vibration.wLeftMotorSpeed  = 60000;
                            Vibration.wRightMotorSpeed = 60000;
                        }
                        XInputSetState(ControllerIndex, &Vibration);
                    }
                    else
                    {
                        // controller no available
                    }
                }

                game_offscreen_buffer Buffer = {};
                Buffer.Memory = GlobalBackBuffer.Memory;
                Buffer.Width = GlobalBackBuffer.Width;
                Buffer.Height = GlobalBackBuffer.Height;
                Buffer.Pitch = GlobalBackBuffer.Pitch;

                GameUpdateAndRender(Buffer, xOffset, yOffset);

                // Direct sound output test
                DWORD PlayCursor;
                DWORD WriteCursor;
                if (SUCCEEDED(GlobalSoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                                       % SoundOutput.SecondaryBufferSize;
                    DWORD BytesToWrite = 0;
                    // TODO: need more accurate check for equivalence
                    //  (can be due to mod and fill a buffer still to be played)
                    if (ByteToLock == PlayCursor)
                    {
                        // fill buffer when starting
                        BytesToWrite = 0;
                    }
                    else if (ByteToLock > PlayCursor)
                    {
                        // split buffer, so circle around
                        BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                        BytesToWrite += PlayCursor;
                    }
                    else
                    {
                        // single buffer, direct size computation
                        BytesToWrite = PlayCursor - ByteToLock;
                    }

                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
                }
                win32_window_dimension Dimension = GetWindowDimension(Window);
                Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);
                ++xOffset;

                MainLoop();

                QueryPerformanceCounter(&EndTime);
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
