#include "handmade.h"

internal void
RenderGradient(game_offscreen_buffer* Buffer, int XOffSet, int YOffset)
{
    u8* Row = (u8*) Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y)
    {
        u32* Pixel = (u32*) Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            u8 Blue  = (X + XOffSet);
            u8 Green = (Y + YOffset);
            *Pixel   = ((Green << 8) | Blue);
            Pixel++;
        }
        Row += Buffer->Pitch;
    }
}

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    RenderGradient(Buffer,XOffset,YOffset);
}