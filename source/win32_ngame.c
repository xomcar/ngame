#include <windows.h>
#include <dwmapi.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>

#include "types.h"
#include "ngame.c"
#include "compiler.h"

#define BUFFER_WIDTH 1280
#define BUFFER_HEIGHT 720

typedef struct
{
    BITMAPINFO info;
    void*      memory;
    int        width;
    int        height;
    int        pitch;
    int        bytesPerPixel;
} win32_offscreen_buffer;

global_variable win32_offscreen_buffer gBackBuffer;
global_variable bool                   gRunning;
global_variable LPDIRECTSOUNDBUFFER    gSoundBuffer;

typedef struct
{
    int width;
    int height;
} win32_window_dimension;

typedef struct
{
    int samplesPerSecond;
    int toneVolume;
    int toneHz;
    int sineWavePeriod;
    int bytesPerSample;
    int secondaryBufferSize;
    int runningSampleIndex;
    f32 tSine;
    int latencySampleCount;
} win32_sound_output;

/* the following allows to create a stub for function in the case the library could not be loaded */
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
    HMODULE xInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!xInputLibrary)
    {
        HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (xInputLibrary)
    {
        XInputGetState = (x_input_get_state*) GetProcAddress(xInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state*) GetProcAddress(xInputLibrary, "XInputSetState");
    }
}

internal win32_window_dimension
GetWindowDimension(HWND Window)
{
    win32_window_dimension result;

    RECT windowRect;
    GetClientRect(Window, &windowRect);
    result.width  = windowRect.right - windowRect.left;
    result.height = windowRect.bottom - windowRect.top;
    return result;
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID device, LPDIRECTSOUND* pSound, LPUNKNOWN other);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32InitDSound(HWND window, i32 samplesPerSecond, i32 bufferSize)
{
    HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");
    if (dSoundLibrary)
    {
        direct_sound_create* DirectSoundCreate
            = (direct_sound_create*) GetProcAddress(dSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND directSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0)))
        {
            WAVEFORMATEX waveFormat    = { 0 };
            waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
            waveFormat.nChannels       = 2;
            waveFormat.nSamplesPerSec  = samplesPerSecond;
            waveFormat.wBitsPerSample  = 16;
            waveFormat.nBlockAlign     = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
            waveFormat.cbSize          = 0;

            if (SUCCEEDED(IDirectSound_SetCooperativeLevel(directSound, window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC bufferDescription = { 0 };
                bufferDescription.dwSize       = sizeof(bufferDescription);
                bufferDescription.dwFlags      = DSBCAPS_PRIMARYBUFFER;
                LPDIRECTSOUNDBUFFER primaryBuffer;
                if (SUCCEEDED(IDirectSound_CreateSoundBuffer(directSound, &bufferDescription, &primaryBuffer, 0)))
                {
                    OutputDebugStringA("Primary audio buffer created\n");
                    if (SUCCEEDED(IDirectSoundBuffer_SetFormat(primaryBuffer, &waveFormat)))
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
            DSBUFFERDESC bufferDescription  = { 0 };
            bufferDescription.dwSize        = sizeof(bufferDescription);
            bufferDescription.dwFlags       = 0;
            bufferDescription.dwBufferBytes = bufferSize;
            bufferDescription.lpwfxFormat   = &waveFormat;
            if (SUCCEEDED(IDirectSound_CreateSoundBuffer(directSound, &bufferDescription, &gSoundBuffer, 0)))
            {
                // start playing audio!
                OutputDebugStringA("Secondary audio buffer created\n");
            }
        }
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output* soundOutput,
                     DWORD               byteToLock,
                     DWORD               bytesToWrite,
                     game_sound_buffer*  sourceBuffer)
{
    VOID* region1;
    DWORD region1Size;
    VOID* region2;
    DWORD region2Size;
    if (SUCCEEDED(IDirectSoundBuffer8_Lock(
            gSoundBuffer, byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0)))
    {
        // TODO: assert region1/region2 size are valid (must be multiple of 32 bit due to 2ch * 16bit)
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        i16*  destSample         = (i16*) region1;
        i16*  sourceSample       = sourceBuffer->samples;
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        };

        destSample               = (i16*) region2;
        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        };
    }

    IDirectSoundBuffer8_Unlock(gSoundBuffer, region1, region1Size, region2, region2Size);
}

internal void
Win32ClearBuffer(win32_sound_output* output)
{
    VOID* region1;
    DWORD region1Size;
    VOID* region2;
    DWORD region2Size;
    if (SUCCEEDED(IDirectSoundBuffer8_Lock(
            gSoundBuffer, 0, output->secondaryBufferSize, &region1, &region1Size, &region2, &region2Size, 0)))
    {
        // TODO: assert region1/region2 size are valid (must be multiple of 32 bit due to 2ch * 16bit)
        u8* destSample = (u8*) region1;
        for (DWORD index = 0; index < region1Size; ++index)
        {
            *destSample++ = 0;
        };
        destSample = (u8*) region2;
        for (DWORD index = 0; index < region2Size; ++index)
        {
            *destSample++ = 0;
        };
    }
    IDirectSoundBuffer8_Unlock(gSoundBuffer, region1, region1Size, region2, region2Size);
}

internal void
Win32InitVideo(win32_offscreen_buffer* buffer, int width, int height)
{
    if (buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->height        = height;
    buffer->width         = width;
    buffer->bytesPerPixel = 4;

    buffer->info.bmiHeader.biSize        = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth       = width;
    buffer->info.bmiHeader.biHeight      = -height;  // negative to have top down y direction
    buffer->info.bmiHeader.biPlanes      = 1;
    buffer->info.bmiHeader.biBitCount    = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = (width * height) * buffer->bytesPerPixel;
    buffer->memory       = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    buffer->pitch        = width * buffer->bytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(HDC deviceContext, int windowWidth, int windowHeight, win32_offscreen_buffer* buffer)
{
    // TODO: correct aspect ratio
    StretchDIBits(deviceContext,
                  // X, Y, Width, Height,
                  // X, Y, Width, Height,
                  0,
                  0,
                  windowWidth,
                  windowHeight,
                  0,
                  0,
                  buffer->width,
                  buffer->height,
                  buffer->memory,
                  &buffer->info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND window, UINT message, WPARAM wParameter, LPARAM lParameter)
{
    LRESULT result = 0;

    switch (message)
    {
        case WM_DESTROY:
        {
            // TODO: handle this as and error, recreate window?
            gRunning = false;
        }
        break;

        case WM_CLOSE:
        {
            // TODO: handle with message to the user?
            gRunning = false;
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
            u32  vKeyCode = wParameter;
            bool wasDown  = ((lParameter & (1 << 30)) != 0);  // check if previously pressed
            bool isDown   = ((lParameter & (1 << 31)) == 0);  // check if pressed
            if (wasDown != isDown)                            // skip input if key is pressed continously
            {
                if (vKeyCode == 'W') {}
                else if (vKeyCode == 'A') {}
                else if (vKeyCode == 'S') {}
                else if (vKeyCode == 'D') {}
                else if (vKeyCode == 'Q') {}
                else if (vKeyCode == 'E') {}
                else if (vKeyCode == VK_ESCAPE)
                {
                    if (isDown)
                    {
                        OutputDebugStringA("Pressed ESC\n");
                    }
                    if (wasDown)
                    {
                        OutputDebugStringA("Unpressed ESC\n");
                    }
                }
                else if (vKeyCode == VK_SPACE) {}
            }

            bool32 altKeyWasDown = (lParameter & (1 << 29));
            if ((vKeyCode == VK_F4) && altKeyWasDown)
            {
                gRunning = false;
            }
        }
        break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC         deviceContext = BeginPaint(window, &paint);

            win32_window_dimension dimension = GetWindowDimension(window);
            Win32DisplayBufferInWindow(deviceContext, dimension.width, dimension.height, &gBackBuffer);
            EndPaint(window, &paint);
        }
        break;

        default:
        {
            // OutputDebugStringA("WM_UNHANDLED_DEFAULT\n");
            result = DefWindowProc(window, message, wParameter, lParameter);
        }
        break;
    }
    return (result);
}

int APIENTRY
WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int showCode)
{
    WNDCLASSA windowClass = { 0 };
    Win32LoadXInput();
    Win32InitVideo(&gBackBuffer, BUFFER_WIDTH, BUFFER_HEIGHT);

    windowClass.style       = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance   = instance;
    // TODO: WindowClass.hIcon;
    windowClass.lpszClassName = "NGameWindowClass";

    if (RegisterClassA(&windowClass))
    {
        HWND Window = CreateWindowExA(0,
                                      windowClass.lpszClassName,
                                      "NGame",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      0,
                                      0,
                                      instance,
                                      0);

        BOOL    enableDarkMode = true;
        HRESULT ret
            = DwmSetWindowAttribute(Window, DWMWA_USE_IMMERSIVE_DARK_MODE, &enableDarkMode, sizeof(enableDarkMode));
        if (Window)
        {
            HDC deviceContext = GetDC(Window);

            int xOffset = 0;
            int yOffset = 0;

            win32_sound_output soundOutput = { 0 };

            soundOutput.samplesPerSecond    = 48000;
            soundOutput.toneHz              = 256;
            soundOutput.toneVolume          = 3000;
            soundOutput.sineWavePeriod      = soundOutput.samplesPerSecond / soundOutput.toneHz;
            soundOutput.bytesPerSample      = sizeof(i16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.latencySampleCount  = soundOutput.samplesPerSecond / 15;
            Win32InitDSound(Window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
            Win32ClearBuffer(&soundOutput);
            IDirectSoundBuffer8_Play(gSoundBuffer, 0, 0, DSBPLAY_LOOPING);

            gRunning = true;

            i16* samples
                = (i16*) VirtualAlloc(0, soundOutput.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            LARGE_INTEGER lastTick, endTick;
            LARGE_INTEGER ticksFrequency;

            QueryPerformanceCounter(&lastTick);
            QueryPerformanceFrequency(&ticksFrequency);
            // Main rendering loop
            while (gRunning)
            {
                QueryPerformanceCounter(&lastTick);
                MSG message;
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        gRunning = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                // TODO: GetState is bugged and hangs for milliseconds if no controller is
                // plugged
                for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex)
                {
                    XINPUT_STATE controllerState;
                    if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
                    {
                        // controller available
                        // TODO investigate dwPacketNumber
                        XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

                        BOOL up    = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        BOOL down  = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        BOOL left  = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        BOOL right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        BOOL start = pad->wButtons & XINPUT_GAMEPAD_START;
                        BOOL back  = pad->wButtons & XINPUT_GAMEPAD_BACK;
                        BOOL a     = pad->wButtons & XINPUT_GAMEPAD_A;
                        BOOL b     = pad->wButtons & XINPUT_GAMEPAD_B;
                        BOOL x     = pad->wButtons & XINPUT_GAMEPAD_X;
                        BOOL y     = pad->wButtons & XINPUT_GAMEPAD_Y;

                        i16 leftStickX  = pad->sThumbLX;
                        i16 rightStickY = pad->sThumbRY;

                        xOffset += leftStickX / 4096;
                        yOffset -= rightStickY / 4096;


                        XINPUT_VIBRATION vibration = { 0 };
                        if (a)
                        {
                            vibration.wLeftMotorSpeed  = 60000;
                            vibration.wRightMotorSpeed = 60000;
                        }
                        XInputSetState(controllerIndex, &vibration);

                        soundOutput.toneHz         = 512 + (int) (256.0f * (f32) rightStickY / 30000.0f);
                        soundOutput.sineWavePeriod = soundOutput.samplesPerSecond / soundOutput.toneHz;
                    }
                    else
                    {
                        // controller no available
                    }
                }


                // Direct sound output test
                DWORD  playCursor   = 0;
                DWORD  writeCursor  = 0;
                DWORD  targetCursor = 0;
                DWORD  byteToLock   = 0;
                DWORD  bytesToWrite = 0;
                bool32 soundIsValid = false;
                if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(gSoundBuffer, &playCursor, &writeCursor)))
                {
                    byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample)
                                 % soundOutput.secondaryBufferSize;
                    targetCursor = (playCursor + (soundOutput.latencySampleCount * soundOutput.bytesPerSample))
                                   % soundOutput.secondaryBufferSize;
                    if (byteToLock > targetCursor)
                    {
                        // split buffer, so circle around
                        bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                        bytesToWrite += targetCursor;
                    }
                    else
                    {
                        // single buffer, direct size computation
                        bytesToWrite = targetCursor - byteToLock;
                    }

                    soundIsValid = true;
                }

                game_sound_buffer soundBuffer = { 0 };
                soundBuffer.samplesPerSecond  = soundOutput.samplesPerSecond;
                soundBuffer.sampleCount       = bytesToWrite / soundOutput.bytesPerSample;
                soundBuffer.samples           = samples;

                game_offscreen_buffer videoBuffer = { 0 };
                videoBuffer.memory                = gBackBuffer.memory;
                videoBuffer.width                 = gBackBuffer.width;
                videoBuffer.height                = gBackBuffer.height;
                videoBuffer.pitch                 = gBackBuffer.pitch;
                GameUpdateAndRender(&videoBuffer, &soundBuffer, xOffset, yOffset, soundOutput.toneHz);

                if (soundIsValid)
                {
                    Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
                }

                win32_window_dimension dimension = GetWindowDimension(Window);
                Win32DisplayBufferInWindow(deviceContext, dimension.width, dimension.height, &gBackBuffer);

                QueryPerformanceCounter(&endTick);
                u64 cyclesElapsed = endTick.QuadPart - lastTick.QuadPart;
                f64 frametimeMS   = 1000.0f * (f64) cyclesElapsed / (f64) ticksFrequency.QuadPart;
                lastTick          = endTick;
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
