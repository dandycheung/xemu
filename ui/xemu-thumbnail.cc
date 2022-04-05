/*
 * xemu User Interface
 *
 * Copyright (C) 2020-2022 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdint>
#include <fpng.h>
#include <vector>

#include "xemu-snapshots.h"
#include "xui/gl-helpers.hh"

static GLuint display_tex = 0;
static bool display_flip = false;

void xemu_snapshots_set_framebuffer_texture(GLuint tex, bool flip)
{
    display_tex = tex;
    display_flip = flip;
}

void xemu_snapshots_render_thumbnail(GLuint tex, TextureBuffer *thumbnail)
{
    std::vector<uint8_t> pixels;
    unsigned int width, height, channels;
    if (fpng::fpng_decode_memory(
            thumbnail->buffer, thumbnail->size, pixels, width, height, channels,
            thumbnail->channels) != fpng::FPNG_DECODE_SUCCESS) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, pixels.data());
}

TextureBuffer *xemu_snapshots_extract_thumbnail()
{
    /*
     * Avoids crashing if a snapshot is made on a thread with no GL context
     * Normally, this is not an issue, but it is better to fail safe than assert
     * here.
     * FIXME: Allow for dispatching a thumbnail request to the UI thread to
     * remove this altogether.
     */
    if (!SDL_GL_GetCurrentContext() || display_tex == 0) {
        return NULL;
    }

    // Render at 2x the base size to account for potential UI scaling
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, display_tex);
    int thumbnail_width = XEMU_SNAPSHOT_WIDTH * 2;
    int tex_width;
    int tex_height;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_height);
    int thumbnail_height = (int)(((float)tex_height / (float)tex_width) * (float)thumbnail_width);

    std::vector<uint8_t> png;
    if (!ExtractFramebufferPixels(display_tex, display_flip, png, thumbnail_width, thumbnail_height)) {
        return NULL;
    }

    TextureBuffer *thumbnail = (TextureBuffer *)g_malloc(sizeof(TextureBuffer));
    thumbnail->buffer = g_malloc(png.size() * sizeof(uint8_t));

    thumbnail->channels = 3;
    thumbnail->size = png.size() * sizeof(uint8_t);
    memcpy(thumbnail->buffer, png.data(), thumbnail->size);
    return thumbnail;
}