#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <functional>

#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"

#include "sdl_util.hpp"

#define SCREEN_WIDTH  1200
#define SCREEN_HEIGHT 480
#define SCREEN_DEPTH  32

// Endianess check for SDL RGBA surfaces
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RMASK 0xff000000
#define GMASK 0x00ff0000
#define BMASK 0x0000ff00
#define AMASK 0x000000ff
#else
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000
#endif

// The size in pixels of a single cell
const unsigned int MAP_SIZE = 8;

const unsigned int MAP_WIDTH = SCREEN_WIDTH / MAP_SIZE;
const unsigned int MAP_HEIGHT = SCREEN_HEIGHT / MAP_SIZE;

template <class T, size_t ROW, size_t COL>
using Map = std::array<std::array<T, COL>, ROW>;


class FlappyFuch
{
    public:
        
        FlappyFuch()
        {
            frames[0] = {
                .x = 263,
                .y = 63,
                .w = 17,
                .h = 12
            };

            frames[1] = {
                .x = 263,
                .y = 89,
                .w = 17,
                .h = 12
            };

            current_frame = 0;
            threshold = 0.0f;
            next_frame = 0;
            last_tick = 0;
            y_v = 30;
            angle = 0;

            x = SCREEN_WIDTH / 12;
            y = 0;
        }

        /**
         * Make the flappy bird flap his flappy wings
         *
         */
        void flap()
        {
            y_v = -250;
        }

        void draw(SDL_Renderer *renderer)
        {
            SDL_Rect dest = {
                .x = (int)x,
                .y = (int)y,
                .w = 38,
                .h = 24
            };

            if (texture) {
                sp::render_texture(renderer, texture, dest,
                                   &frames[current_frame], angle);
            }
        }

        void update(int tick)
        {
            float t = ((tick - last_tick) / 1000.0f);
            y += y_v * t;
            y_v += 920 * t;
            angle += ((y_v / 12.0) - angle) * t * 20;
            if (angle >= 360 || angle <= -360)
                angle = 0;

            if (y <= 0.0f)
                y = 0.0f;

            if (y >= SCREEN_HEIGHT - frames[current_frame].h - 10 - 60) {
                y = SCREEN_HEIGHT - frames[current_frame].w - 10 - 60;
                if (angle < 90)
                    angle += (90 - angle) * t * 52;
                else
                    angle = 90;
            }

            if (next_frame <= tick) {
                current_frame = (current_frame + 1) % 2;
                next_frame = tick + 100;
            }

            last_tick = tick;
        }

        void set_texture(SDL_Texture *texture)
        {
            this->texture = texture;
        }

        void die()
        {

        }

    private:
        SDL_Texture *texture;
        SDL_Rect frames[2];
        int current_frame;
        float x, y, y_v;
        int next_frame, last_tick;
        float threshold;
        double angle;
};


class Obstacle
{
    public:
        
        Obstacle(float x, float y, int begin, int end, int gap): begin(begin),
                                                             end(end), gap(gap)
        {
            last_tick = 0;
            this->x = x;
            this->y = y;

            clip_pipe_top_body = {
                .x = 303,
                .y = 0,
                .w = 24,
                .h = 1
            };

            clip_pipe_top = {
                .x = 302,
                .y = 123,
                .w = 26,
                .h = 12
            };

            dest_pipe_top = {
                .x = (int)x,
                .y = (int)y - begin,
                .w = 26 * 2,
                .h = 12 * 2
            };

            dest_pipe_top_body = {
                .x = (int)x + 2,
                .y = begin,
                .w = 24 * 2,
                .h = (int)y - begin
            };

            clip_pipe_bottom_body = {
                .x = 331,
                .y = 12,
                .w = 24,
                .h = 1
            };

            clip_pipe_bottom = {
                .x = 330,
                .y = 0,
                .w = 26,
                .h = 12
            };

            dest_pipe_bottom = {
                .x = (int)x,
                .y = begin + gap + (int)y + 12 * 2,
                .w = 26 * 2,
                .h = 12 * 2
            };

            dest_pipe_bottom_body = {
                .x = (int)x + 2,
                .y = begin + gap + (int)y + 12 * 4,
                .w = 24 * 2,
                .h = end - (begin + gap + (int)y + 12 * 4)
            };

        }

        void draw(SDL_Renderer *renderer)
        {
            if (texture) {
                sp::render_texture(renderer, texture, dest_pipe_top_body, &clip_pipe_top_body);
                sp::render_texture(renderer, texture, dest_pipe_top, &clip_pipe_top);

                sp::render_texture(renderer, texture, dest_pipe_top_body, &clip_pipe_bottom_body);
                sp::render_texture(renderer, texture, dest_pipe_top, &clip_pipe_bottom);

                sp::render_texture(renderer, texture, dest_pipe_bottom_body, &clip_pipe_bottom_body);
                sp::render_texture(renderer, texture, dest_pipe_bottom, &clip_pipe_bottom);

                sp::render_texture(renderer, texture, dest_pipe_bottom_body, &clip_pipe_bottom_body);
                sp::render_texture(renderer, texture, dest_pipe_bottom, &clip_pipe_bottom);
            }
        }

        void update(int tick)
        {
            int delta = tick - last_tick;
            x -= 120.0f * (delta / 1000.0f);

            set_x((int)x);

            last_tick = tick;
        }

        void set_x(int x)
        {
            dest_pipe_top.x = x;
            dest_pipe_top_body.x = x + 2;

            dest_pipe_bottom.x = x;
            dest_pipe_bottom_body.x = x + 2;
        }

        void set_height(int y)
        {
            this->y = y;

            dest_pipe_top.y = y - begin;
            dest_pipe_bottom.y = begin + gap + y + 12 * 2;

            dest_pipe_bottom_body.y = begin + gap + y + 12 * 4;
            dest_pipe_bottom_body.h = end - (begin + gap + y + 12 * 4);
        }

        void set_texture(SDL_Texture *texture)
        {
            this->texture = texture;
        }

    private:
        SDL_Texture *texture;

        SDL_Rect clip_pipe_top_body;
        SDL_Rect clip_pipe_top;

        SDL_Rect dest_pipe_top_body;
        SDL_Rect dest_pipe_top;

        SDL_Rect clip_pipe_bottom_body;
        SDL_Rect clip_pipe_bottom;

        SDL_Rect dest_pipe_bottom_body;
        SDL_Rect dest_pipe_bottom;

        int begin, end, gap;
        int last_tick;
        float x, y;
};


/**
 * Figures out the endianess and applies the pixel color to the surface
 */
static inline void put_pixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
        case 1:
            *p = pixel;
            break;

        case 2:
            *(Uint16 *)p = pixel;
            break;

        case 3:
            if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                p[0] = (pixel >> 16) & 0xff;
                p[1] = (pixel >> 8) & 0xff;
                p[2] = pixel & 0xff;
            } else {
                p[0] = pixel & 0xff;
                p[1] = (pixel >> 8) & 0xff;
                p[2] = (pixel >> 16) & 0xff;
            }
            break;

        case 4:
            *(Uint32 *)p = pixel;
            break;
    }
}


int main(int argc, char *argv[]) {

    unsigned long ticks = 0, delta = 0;
    unsigned long last_tick = SDL_GetTicks(); 

    bool quit = false;
    bool lock_flap = false;
    SDL_Event event;

    if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
        std::cerr << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Window *win = nullptr;
    win = SDL_CreateWindow("Crappy Bird", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                           SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    if (win == nullptr) {
        std::cerr << SDL_GetError() << std::endl;
        return 1;

    }

    SDL_Renderer *renderer = nullptr;
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED |
                                           SDL_RENDERER_PRESENTVSYNC);

    if (renderer == nullptr) {
        std::cerr << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Surface *jpg = IMG_Load("data/spritesheet.png");
    if (jpg == nullptr) {
        std::cerr << IMG_GetError() << std::endl;
        return 1;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, jpg);
    SDL_FreeSurface(jpg);
    if (tex == nullptr) {
        std::cerr << SDL_GetError() << std::endl;
        return 1;
    }

    FlappyFuch player = FlappyFuch();

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, SCREEN_HEIGHT - 60 - 12 * 4 - 100);
    auto y_gen = std::bind(distribution, generator);

    std::vector<Obstacle> obstacles;

    for(int i = 0; i < 100; i++) {
        Obstacle temp_obs = Obstacle(300 + i * 200, y_gen(), 0, SCREEN_HEIGHT - 60, 100);
        temp_obs.set_texture(tex);
        obstacles.push_back(temp_obs);
    }

    player.set_texture(tex);

    SDL_Rect background = {
        .x = 0,
        .y = 0,
        .w = 143,
        .h = 255
    };

    SDL_Rect background_rects[SCREEN_WIDTH / 143];

    for (int i = 0; i < (SCREEN_WIDTH / 143); i++) {
        background_rects[i] = {
            i * 143, 0, 143 * 2, SCREEN_HEIGHT - 60
        };
    }

    int num_ground = SCREEN_WIDTH / (154 * 2) + 1;
    SDL_Rect ground = { 146, 1, 154, 55 };

    SDL_Rect ground_rects[num_ground];

    for (int i = 0; i < num_ground; i++) {
        ground_rects[i] = {i * 154 * 2, 0, 154 * 2, 55 * 2 };
    }

    Uint32 format;
    int access;

    SDL_QueryTexture(tex, &format, &access, NULL, NULL);
    SDL_Texture *ground_texture = SDL_CreateTexture(renderer, format, access |
                                                    SDL_TEXTUREACCESS_TARGET,
                                                    154 * 2 * num_ground, 55 * 2);
    SDL_SetRenderTarget(renderer, ground_texture);
    for (int i = 0; i < num_ground; i++)
        sp::render_texture(renderer, tex, ground_rects[i], &ground);
    SDL_SetRenderTarget(renderer, NULL);

    float ground_x_1 = 0.0f;
    float ground_x_2 = ground_x_1 + 154 * 2 * num_ground;

    while (!quit) {
        delta = SDL_GetTicks() - last_tick;
        last_tick = SDL_GetTicks();
        ticks += delta;

        /*
         * Poll for events, and handle the ones we care about.
         * When a user presses 'n', it handles a single step of a
         * game of life generation
         */
        while (SDL_PollEvent(&event)) 
        {
            switch (event.type) 
            {
                case SDL_KEYUP:                  
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_ESCAPE:
                            quit = true;
                            break;
                    }
                    break;
                case SDL_QUIT:
                    return(0);
            }
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        if (!lock_flap && state[SDL_SCANCODE_SPACE])
        {
            lock_flap = true;
            player.flap();
        } else if(!state[SDL_SCANCODE_SPACE]) {
            lock_flap = false;
        }

        ground_x_1 -= 120.0f * (delta / 1000.0f);
        ground_x_2 -= 120.0f * (delta / 1000.0f);

        if (ground_x_1 <= -(154 * 2 * num_ground))
            ground_x_1 = ground_x_2 + 154 * 2 * num_ground;

        if (ground_x_2 <= -(154 * 2 * num_ground))
            ground_x_2 = ground_x_1 + 154 * 2 * num_ground;

        player.update(last_tick);
        for (std::vector<Obstacle>::iterator it = obstacles.begin(); it != obstacles.end(); it++)
            it->update(last_tick);

        SDL_RenderClear(renderer);

        for (int i = 0; i < (SCREEN_WIDTH / 143); i++)
            sp::render_texture(renderer, tex, background_rects[i], &background);

        sp::render_texture(renderer, ground_texture, (int)ground_x_1, SCREEN_HEIGHT - 60);
        sp::render_texture(renderer, ground_texture, (int)ground_x_2, SCREEN_HEIGHT - 60);

        for (std::vector<Obstacle>::iterator it = obstacles.begin(); it != obstacles.end(); it++)
            it->draw(renderer);

        player.draw(renderer);
        SDL_RenderPresent(renderer);
    }

    
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
