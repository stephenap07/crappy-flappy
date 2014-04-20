#include "SDL2/SDL.h"


namespace sp {

    /**
     * Draw an SDL_Texture to an SDL_Renderer at some destination rect
     * taking a clip of the texture if desired
     * @param rend The renderer we want to draw too
     * @param tex The source texture we want to draw
     * @param dst The destination rectangle to render the texture too
     * @param clip The sub-section of the texture to draw (clipping rect)
     *       default of nullptr draws the entire texture
     */
    void render_texture(SDL_Renderer *ren, SDL_Texture *tex, SDL_Rect dst,
                        SDL_Rect *clip = nullptr, const double angle=0)
    {
        SDL_RenderCopyEx(ren, tex, clip, &dst, angle, nullptr, SDL_FLIP_NONE);
    }

    /**
      * Draw an SDL_Texture to an SDL_Renderer at position x, y, preserving
      * the texture's width and height and taking a clip of the texture if desired
      * If a clip is passed, the clip's width and height will be used instead of
      *    the texture's
      * @param rend The renderer we want to draw too
      * @param tex The source texture we want to draw
      * @param x The x coordinate to draw too
      * @param y The y coordinate to draw too
      * @param clip The sub-section of the texture to draw (clipping rect)
      *        default of nullptr draws the entire texture
      */
    void render_texture(SDL_Renderer *ren, SDL_Texture *tex, int x, int y,
                        SDL_Rect *clip = nullptr, const double angle=0)
    {
        SDL_Rect dst;
        dst.x = x;
        dst.y = y;
        if (clip != nullptr){
            dst.w = clip->w;
            dst.h = clip->h;
        }
        else
            SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);

        render_texture(ren, tex, dst, clip);
    }

}
