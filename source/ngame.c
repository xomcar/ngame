#include "ngame.h"

internal void
RenderGradient(game_offscreen_buffer* buffer, int xOffSet, int yOffset)
{
    u8* row = (u8*) buffer->memory;
    for (int y = 0; y < buffer->height; ++y)
    {
        u32* pixel = (u32*) row;
        for (int x = 0; x < buffer->width; ++x)
        {
            u8 blue  = (u8) (x + xOffSet);
            u8 green = (u8) (y + yOffset);
            *pixel   = ((green << 8) | blue);
            pixel++;
        }
        row += buffer->pitch;
    }
}

internal void
GameOutputSound(game_sound_buffer* soundBuffer, int toneHz)
{
    local_persist f32 tSine;
    i16               toneVolume = 3000;
    int               wavePeriod = soundBuffer->samplesPerSecond / toneHz;

    i16* sampleOut = (i16*) soundBuffer->samples;
    for (u32 sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; ++sampleIndex)
    {
        f32 sineValue   = sinf(tSine);
        i16 sampleValue = (i16) (sineValue * toneVolume);
        *sampleOut++    = sampleValue;
        *sampleOut++    = sampleValue;
        tSine += 2.0f * Pi32 / (f32) wavePeriod;
    }
}

internal void
GameUpdateAndRender(
    game_offscreen_buffer* videoBuffer, game_sound_buffer* soundBuffer, int xOffset, int yOffset, int toneHz)
{
    GameOutputSound(soundBuffer, toneHz);
    RenderGradient(videoBuffer, xOffset, yOffset);
}