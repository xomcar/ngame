#ifndef NGAME_H
#define NGAME_H

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

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
    i16*         samples;
    unsigned int sampleCount;
    unsigned int samplesPerSecond;
};

struct game_button_state
{
    int    halfTransitionCount;
    bool32 endedDown;
};

struct game_controller_input
{
    bool32 isAnalog;

    f32 startX;
    f32 startY;

    f32 endX;
    f32 endY;

    f32 minX;
    f32 minY;

    f32 maxX;
    f32 maxY;

    union
    {
        game_button_state buttons[6];
        struct
        {
            game_button_state up;
            game_button_state down;
            game_button_state left;
            game_button_state right;
            game_button_state rightShoulder;
            game_button_state leftShoulder;
        };
    };
};

struct game_input
{
    game_controller_input controllers[4];
};

/*
    Services the platform layer provides to the game:
*/

#endif