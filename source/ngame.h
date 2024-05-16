#ifndef NGAME_H
#define NGAME_H

#include "types.h"

/*
    Services provided by the game to the platform layer:
*/

// needs timing, input, bitmap to output and sound to use

struct game_offscreen_buffer
{
    void* memory;
    int   width;
    int   height;
    int   pitch;
    int   bytesPerPixel;
};

struct game_sound_buffer
{
    i16* samples;
    int  sampleCount;
    int  samplesPerSecond;
};

internal void
GameUpdateAndRender(game_offscreen_buffer* buffer, game_sound_buffer* soundBuffer, int xOffset, int yOffset, int toneHz);


// TODO: allow sample offset control
internal void
GameOutputSound(game_sound_buffer* SoundBuffer, int toneHz);
/*
    Services the platform layer provides to the game:
*/

#endif