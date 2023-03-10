/***************************************************************************

    M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
    Win32 Portions Copyright (C) 1997-98 Michael Soderstrom and Chris Kirmse
    
    This file is part of MAME32, and may only be used, modified and
    distributed under the terms of the MAME license, in "readme.txt".
    By continuing to use, modify or distribute this file you indicate
    that you have read the license and understand and accept it fully.

 ***************************************************************************/
 
/***************************************************************************

  dirty.c

  Win32 dirty buffer code

 ***************************************************************************/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "dirty.h"

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

/***************************************************************************
    function prototypes
 ***************************************************************************/

/***************************************************************************
    External variables
 ***************************************************************************/

/*
    need two dirty buffers, one for current frame and one for previous frame,
    and redraw if either one is set

    format of the dirty buffer is
        <row><row>...<row> (dirty_height times)
        each <row> is dirty_width dwords long, with each bit meaning 1 pixel
        is dirty
*/
   
int *dirty_buffer1; 
int *dirty_buffer2; 
int *dirty_line1;
int *dirty_line2;

/* screen size, rounded up because we're storing 1 bit per pixel */
int dirty_width; /* in DWORDs */
int dirty_height; /* in pixel height */

/***************************************************************************
    Internal structures
 ***************************************************************************/

/***************************************************************************
    Internal variables
 ***************************************************************************/

static int *current_dirty_buffer;
static int *current_dirty_line;
static int clip_width;
static enum DirtyMode dirty_mode;

/***************************************************************************
    External functions  
 ***************************************************************************/

void InitDirty(int width, int height, enum DirtyMode eDirtyMode)
{
    dirty_mode = eDirtyMode;

    if (dirty_mode == NO_DIRTY)
        return;

    if (dirty_mode == USE_DIRTYRECT)
    {
        clip_width = width;
        dirty_width = (width + 31)/32; /* round up, since one bit per pixel in dirty_buffer */
        dirty_height = height;
    
        dirty_buffer1 = (int *)malloc(dirty_width*4 * dirty_height);
        dirty_buffer2 = (int *)malloc(dirty_width*4 * dirty_height);
    
        current_dirty_buffer = dirty_buffer1;
    
        dirty_line1 = (int *)malloc(dirty_height*sizeof(int));
        dirty_line2 = (int *)malloc(dirty_height*sizeof(int));

        current_dirty_line = dirty_line1;    
    
        /* clear the buffers */
        SwitchDirty();
        SwitchDirty();
    }
    else
    {
        dirty_mode = NO_DIRTY;
    }
}

void ExitDirty(void)
{
    if (dirty_mode == USE_DIRTYRECT)
    {
        free(dirty_buffer1);
        free(dirty_buffer2);
    
        dirty_buffer1 = NULL;
        dirty_buffer2 = NULL;
    
        current_dirty_buffer = NULL;
    
        free(dirty_line1);
        free(dirty_line2);
        dirty_line1 = NULL;
        dirty_line2 = NULL;

        current_dirty_line = NULL;
    }

    dirty_mode = NO_DIRTY;
}

void MarkDirty(int x1, int y1, int x2, int y2, int ui)
{
    if (dirty_mode == NO_DIRTY)
        return;

    if (dirty_mode == USE_DIRTYRECT)
    {
        int i,j;
        int ones,mask,offset;

        /* clip to display */
    
        if (clip_width <= x1)
            x1 = clip_width - 1;
        if (x1 < 0)
            x1 = 0;

        if (clip_width <= x2)
            x2 = clip_width - 1;
        if (x2 < 0)
            x2 = 0;
    
        if (dirty_height <= y1)
            y1 = dirty_height - 1;
        if (y1 < 0)
            y1 = 0;

        if (dirty_height <= y2)
            y2 = dirty_height - 1;
        if (y2 < 0)
            y2 = 0;
    
        for (i=y1;i<=y2;i++)
        {
            current_dirty_line[i] += x2-x1+1;
            j = x1;
            while (j <= x2)
            {
                offset = j % 32;
            
                ones = min((32 - offset),x2-j+1);
                mask = current_dirty_buffer[DIRTYDWORD(j,i)];
                if (ones == 32)
                    mask = 0xffffffff;
                else
                    mask |= ((1 << ones) - 1) << offset;
            
#ifdef DEBUG
                if (DIRTYDWORD(j,i) >= dirty_width * dirty_height)
                    printf("death\n");
#endif
                current_dirty_buffer[DIRTYDWORD(j,i)] = mask;
                j += 32 - offset;
            }
        }
    }
}

void MarkDirtyPixel(int x, int y)
{
    if (dirty_mode == NO_DIRTY)
        return;

    if (dirty_mode == USE_DIRTYRECT)
    {
        int ones, mask, offset;
   
        /* clip to display */
    
        if (clip_width <= x)
            x = clip_width - 1;
        if (x < 0)
            x = 0;

        if (clip_width <= x)
            x = clip_width - 1;
        if (x < 0)
            x = 0;
    
        if (dirty_height <= y)
            y = dirty_height - 1;
        if (y < 0)
            y = 0;

        if (dirty_height <= y)
            y = dirty_height - 1;
        if (y < 0)
            y = 0;
    
        current_dirty_line[y]++;
        offset = x % 32;
            
        ones = (32 - offset);
        mask = current_dirty_buffer[DIRTYDWORD(x,y)];
        if (ones == 32)
            mask = 0xffffffff;
        else
            mask |= ((1 << ones) - 1) << offset;
            
#ifdef DEBUG
                if (DIRTYDWORD(x, y) >= dirty_width * dirty_height)
                    printf("death\n");
#endif
        current_dirty_buffer[DIRTYDWORD(x, y)] = mask;
    }
}


void SwitchDirty(void)
{
    if (dirty_mode == NO_DIRTY)
        return;
    
    if (dirty_mode == USE_DIRTYRECT)
    {
        if (current_dirty_buffer == dirty_buffer1)
        {
            current_dirty_buffer = dirty_buffer2;
            current_dirty_line = dirty_line2;
        }
        else
        {
            current_dirty_buffer = dirty_buffer1;
            current_dirty_line = dirty_line1;
        }
    
        memset(current_dirty_buffer,0,dirty_width*4*dirty_height);
        memset(current_dirty_line,0,4*dirty_height);
    }
}
