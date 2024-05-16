#ifndef NGAME_H
#define NGAME_H

/* 
    Services provided by the game to the platform layer:
*/

// needs timing, input, bitmap to output and sound to use

struct game_offscreen_buffer
{
    BITMAPINFO Info;
    void*      Memory;
    int        Width;
    int        Height;
    int        Pitch;
    int        BytesPerPixel;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset);

/*
    Services the platform layer provides to the game:
*/

#endif