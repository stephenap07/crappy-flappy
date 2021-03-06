#include <iostream>
#include <vector>
#include <random>
#include <functional>
#include <chrono>
#include <fstream>
#include <algorithm>

#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_mixer.h"

#include "sdl_util.hpp"

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 360
#define SCREEN_DEPTH  32

#define SPEED 200.0f

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


Mix_Chunk *g_score = nullptr;

class Entity
{
    public:
        Entity()
        {
            id = generate_id();
        }

        virtual void update(int delta) = 0;
        virtual void draw(SDL_Renderer *renderer) = 0;
        virtual void on_collision(Entity *ent) {};

        bool operator==(const Entity &ent) const
        {
            return (id == ent.get_id());
        }

        std::vector<SDL_Rect> get_collision_rects()
        {
            return collision_rects;
        }

        const int get_id() const
        {
            return id;
        }

    protected:
        int id;
        SDL_Texture *texture;
        double angle;
        std::vector<SDL_Rect> collision_rects;

    private:
        int generate_id()
        {
            static int s_nid = 0;
            return s_nid++;
        } 
};


class CollisionBank
{
    public:

        void register_entity(Entity *entity)
        {
            entities.push_back(entity);
        }

        void dispatch_collisions()
        {
            /* N^2 does it matter for small amount of entities? */
            for (Entity * entity_a : entities) {
                for (Entity * entity_b : entities) {
                    if (*entity_a == *entity_b)
                        continue;

                    for(SDL_Rect rect_a : entity_a->get_collision_rects()) {
                        for(SDL_Rect rect_b : entity_b->get_collision_rects()) {
                            if (check_collision(rect_a, rect_b)) {
                                entity_b->on_collision(entity_a);
                                entity_a->on_collision(entity_b);
                            }
                        }
                    }
                }
            }
        }

        
        static bool check_collision(SDL_Rect A, SDL_Rect B) {
            // The sides of the rectangles
            int leftA, leftB;
            int rightA, rightB;
            int topA, topB;
            int bottomA, bottomB;

            // Calculate the sides of rect A
            leftA = A.x;
            rightA = A.x + A.w;
            topA = A.y;
            bottomA = A.y + A.h;

            // Calculate the sides of rect B
            leftB = B.x;
            rightB = B.x + B.w;
            topB = B.y;
            bottomB = B.y + B.h; 
                
            // If any of the sides from A are outside of B
            if(bottomA <= topB) {
                return false;
            }
            if(topA >= bottomB) {
               return false;
            }
            if(rightA <= leftB) {
               return false;
            }
            if(leftA >= rightB) {
                return false;
            }

            // If none of the sides from A are outside B
            return true;
        } 

        static  bool check_point_collision(int x, int y, SDL_Rect rect)
        {
            if (x >= rect.x && x <= (rect.x + rect.w) &&
                y >= rect.y && y <= (rect.y + rect.h))
                return true;
            else
                return false;
        }

    private:
        std::vector<Entity*> entities;
};


class FlappyFuch :public Entity
{
    public:
        
        FlappyFuch(int x, int y) :Entity(), x(x), y(y)
        {

            frames[0] = {
                .x = 264,
                .y = 64,
                .w = 17,
                .h = 12
            };

            frames[1] = {
                .x = 264,
                .y = 90,
                .w = 17,
                .h = 12
            };

            frames[2] = {
                .x = 223,
                .y = 124,
                .w = 17,
                .h = 12
            };

            frames[3] = frames[1];

            current_frame = 0;

            threshold = 0.0f;
            next_frame = 0;
            last_tick = 0;
            y_v = 30;
            angle = 0;

            dead = false;
            score_queued = false;
            score_count = 0;
            idle = false;

            set_collision();
        }

        void flap()
        {
            if (!dead && !idle)
                y_v = -265;
        }

        SDL_Rect get_dest()
        {
            return collision_rects[0];
        }

        void set_collision()
        {
            if (!collision_rects.empty())
                collision_rects.pop_back();
            collision_rects.push_back({
                .x = (int)x,
                .y = (int)y,
                .w = 38,
                .h = 24
            });
        }

        void draw(SDL_Renderer *renderer)
        {
            if (texture) {
                sp::render_texture(renderer, texture, get_dest(),
                                   &frames[current_frame], angle);
            }
        }

        void update(int delta)
        {
            if (score_queued && !in_collision && !idle && !dead) {
                score_count++;
                score_queued = false;
                if (Mix_PlayChannel(-1, g_score, 0) == -1 ) {
                    std::cerr << "Mix_PlayChannel: " << Mix_GetError() << std::endl;
                }
            }

            int acceleration = 920;
            float t = (delta / 1000.0f);
            
            if (dead)
                acceleration = 5000;

            if (!idle) {
                y += y_v * t;
                y_v += acceleration * t;
                if (y <= 0.0f)
                    y = 0.0f;
            }

            if (y >= SCREEN_HEIGHT - frames[current_frame].h - 10 - 60) {
                die();
                y = SCREEN_HEIGHT - frames[current_frame].w - 10 - 60;
                if (angle < 90)
                    angle += (90 - angle) * t * 52;
                else
                    angle = 90;
            } else if (!idle) {
                angle += ((y_v / 10.0) - angle) * t * 15;
                if (angle >= 360 || angle <= -360)
                    angle = 0;
            }

            if (!dead) {
                if (next_frame >= 60) {
                    current_frame = (current_frame + 1) % 4;
                    next_frame = 0;
                }
                next_frame += delta;
            } else {
                current_frame = 0;
            }

            set_collision();

            in_collision = false;
        }

        void set_texture(SDL_Texture *texture)
        {
            this->texture = texture;
        }

        void set_idle()
        {
            idle = true;
        }

        void set_active()
        {
            idle = false;
        }

        bool is_idle()
        {
            return idle;
        }

        void die()
        {
            dead = true;
        }

        void on_collision(Entity *ent1)
        {
            in_collision = true;                
        }

        void score()
        {
            score_queued = true;
        }

        int get_score()
        {
            return score_count;
        }

        bool is_dead()
        {
            return dead;
        }

        void set_alive()
        {
            dead = false;
            score_count = 0;
            in_collision = false;
            score_queued = false;
            angle = 0;
        }

        void set_y(int y)
        {
            this->y = y;
            set_collision();
        }

    private:
        SDL_Rect frames[4];
        int current_frame;
        float x, y, y_v;
        int next_frame, last_tick;
        int score_count;
        float threshold;
        bool dead, score_queued, in_collision;
        bool idle;
};


class Obstacle :public Entity
{
    public:
        
        Obstacle(float x, float y, int begin, int end, int gap)
            : Entity(), begin(begin), end(end), gap(gap)
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

        void update(int delta)
        {
            set_x(x - SPEED * (delta / 1000.0f));
        }

        void set_x(float x)
        {
            this->x = x;

            dest_pipe_top.x = (int)x;
            dest_pipe_top_body.x = (int)x + 2;

            dest_pipe_bottom.x = (int)x;
            dest_pipe_bottom_body.x = (int)x + 2;

            set_collision_rects();
        }
        
        void set_collision_rects()
        {
            if (!collision_rects.empty())
                collision_rects.clear();

            collision_rects.push_back(dest_pipe_top);
            collision_rects.push_back(dest_pipe_top_body);
            collision_rects.push_back(dest_pipe_bottom);
            collision_rects.push_back(dest_pipe_bottom_body);

            SDL_Rect score_rect = {
                dest_pipe_top.x,
                dest_pipe_top.y + dest_pipe_top.h,
                1,
                dest_pipe_bottom.y - dest_pipe_top.y + dest_pipe_top.h
            };
            
            collision_rects.push_back(score_rect);
        }

        void set_height(int y)
        {
            this->y = y;

            dest_pipe_top.y = y - begin;
            dest_pipe_top_body.h = y - begin;

            dest_pipe_bottom.y = begin + gap + y + 12 * 2;
            dest_pipe_bottom_body.y = begin + gap + y + 12 * 4;
            dest_pipe_bottom_body.h = end - (begin + gap + y + 12 * 4);

            set_collision_rects();
        }

        void on_collision(Entity *entity)
        {
            try {
                FlappyFuch *player = dynamic_cast<FlappyFuch*>(entity);
                if (player != nullptr) {
                    std::vector<SDL_Rect> player_rects = player->get_collision_rects();
                    for(std::vector<SDL_Rect>::iterator it =
                         collision_rects.begin(); it < collision_rects.end() - 1; it++) {

                        if (CollisionBank::check_collision(*it, player_rects[0])) {
                            player->die(); 
                            return;
                        }
                    }
                    
                    player->score();
                }
            } catch(std::bad_cast) {
            }
        }

        void set_texture(SDL_Texture *texture)
        {
            this->texture = texture;
        }

        int get_width()
        {
            return dest_pipe_top.w;
        }

        int get_x()
        {
            return x;
        }

    private:

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


static std::vector<int> digit_to_array(int digit)
{
    std::vector<int> result;
    std::vector<int>::iterator it;

    while (digit) {
        it = result.begin();
        result.insert(it, digit % 10);
        digit /= 10;
    }

    return result;
}

int main(int argc, char *argv[]) {

    unsigned long ticks = 0, delta = 0;
    unsigned long last_tick = SDL_GetTicks(); 

    bool quit = false;
    bool lock_flap = false;
    SDL_Event event;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1) {
        std::cerr << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    int audio_rate = 22050;
    Uint16 audio_format = AUDIO_S16SYS;
    int audio_channels = 2;
    int audio_buffers = 4096;

    // Initialize SDL_mixer
    if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) == -1) {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
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

    g_score = Mix_LoadWAV("data/score.wav");
    if(g_score == NULL) {
        printf("Failed to load scratch sound effect! SDL_mixer Error: %s\n", Mix_GetError());
        SDL_Quit();
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

    FlappyFuch player = FlappyFuch(SCREEN_WIDTH / 12,
                                   SCREEN_HEIGHT / 2 - 60);

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> distribution(0, SCREEN_HEIGHT - 60 - 12 * 4 - 90);
    auto y_gen = std::bind(distribution, generator);

    std::vector<Obstacle> obstacles;

    for(int i = 0; i < (SCREEN_WIDTH / 100); i++) {
        Obstacle temp_obs = Obstacle(600 + i * 200, y_gen(), 0, SCREEN_HEIGHT - 60, 90);
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
    SDL_Rect ground = { 146, 0, 154, 55 };

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
    Obstacle & last = obstacles.back();

    CollisionBank col_bank;
    col_bank.register_entity(&player);

    for (Entity & i : obstacles) {
        col_bank.register_entity(&i);
    }

    SDL_Rect numbers[20] = {
        { 288, 100, 8, 10 }, // 0
        { 288, 118, 8, 10 }, // 1
        { 288, 134, 8, 10 }, // 2
        { 288, 150, 8, 10 }, // 3
        { 287, 173, 8, 10 }, // 4
        { 287, 185, 8, 10 }, // 5
        { 165, 245, 8, 10 }, // 6
        { 175, 245, 8, 10 }, // 7
        { 185, 245, 8, 10 }, // 8
        { 195, 245, 8, 10 }, // 9

        { 288, 74,  6, 7 },  // 0_i
        { 289, 162, 6, 7 },  // 1_i
        { 204, 245, 6, 7 },  // 2_i
        { 212, 245, 6, 7 },  // 3_i
        { 220, 245, 6, 7 },  // 4_i
        { 228, 245, 6, 7 },  // 5_i
        { 284, 197, 6, 7 },  // 6_i
        { 292, 197, 6, 7 },  // 7_i
        { 284, 213, 6, 7 },  // 8_i
        { 292, 213, 6, 7 },  // 9_i
    };

    bool mouse_down = false;

    SDL_Rect game_over_src = {
        146, 199, 94, 19
    };

    SDL_Rect game_over_dest = {
        SCREEN_WIDTH / 2 - 94,
        SCREEN_HEIGHT / 2 - 19 * 5,
        94 * 2,
        19 * 2
    };

    SDL_Rect score_board_src = {
        146, 58, 113, 58
    };

    SDL_Rect score_board_dest = {
        SCREEN_WIDTH / 2 - 113,
        SCREEN_HEIGHT / 2 - 19 * 2,
        113 * 2,
        58 * 2
    };

    SDL_Rect ok_src = {
        246, 134, 40, 14
    };

    SDL_Rect ok_dest = {
        40, SCREEN_HEIGHT - 100, 40 * 2, 14 * 2
    };

    SDL_Rect tap_src = {
        160, 117, 48, 53
    };

    SDL_Rect tap_dest = {
        SCREEN_WIDTH / 2 - 48,
        SCREEN_HEIGHT / 2 - 53,
        48 * 2,
        53 * 2
    };

    bool ok_hot = false;
    bool ok_active = false;

    bool set_best_score = false;

    player.set_idle();

    std::ifstream high_score_fs_in;
    std::ofstream high_score_fs;

    high_score_fs.open("highscore", std::fstream::out | std::fstream::app);
    high_score_fs_in.open("highscore", std::fstream::in);

    if (high_score_fs_in.fail() || high_score_fs.fail() ) {
        std::cerr << "Failed to open highscore file\n";
        return 1;
    }

    std::vector<int> high_score_list;
    int tmp_score;
    while (high_score_fs_in >> tmp_score) {
        high_score_list.push_back(tmp_score);
    }

    high_score_fs_in.close();

    int best_score = 0;
    if (high_score_list.size()) {
        best_score = *std::max_element(high_score_list.begin(),
                                       high_score_list.end());
    };

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

                case SDL_MOUSEBUTTONDOWN:
                    mouse_down = true;
                    break;

                case SDL_MOUSEBUTTONUP:
                    mouse_down = false;
                    break;

                case SDL_QUIT:
                    quit = true;
            }
        }


        const Uint8 *state = SDL_GetKeyboardState(NULL);
        if (!lock_flap && state[SDL_SCANCODE_SPACE])
        {
            player.set_active();
            lock_flap = true;
            player.flap();
        } else if(!state[SDL_SCANCODE_SPACE]) {
            lock_flap = false;
        }

        col_bank.dispatch_collisions();

        if (!player.is_dead()) {
            ground_x_1 -= SPEED * (delta / 1000.0f);
            ground_x_2 -= SPEED * (delta / 1000.0f);
        }

        if (ground_x_1 <= -(154 * 2 * num_ground))
            ground_x_1 = ground_x_2 + 154 * 2 * num_ground;

        if (ground_x_2 <= -(154 * 2 * num_ground))
            ground_x_2 = ground_x_1 + 154 * 2 * num_ground;

        player.update(delta);

        if (!player.is_idle() && !player.is_dead()) {
            for (std::vector<Obstacle>::iterator it = obstacles.begin(); it != obstacles.end(); it++) {
                it->update(delta);
                if (it->get_x() + it->get_width() <= 0) {
                    it->set_x(last.get_x() + 200);
                    it->set_height(y_gen());
                    last = *it;
                }
            }
        }

        std::vector<int> score_array = digit_to_array(player.get_score());
        std::vector<SDL_Rect> score_dest_rect;

        for (int i = 0; i < score_array.size(); i++) {
            SDL_Rect dest = {
                (int)((SCREEN_WIDTH / 2) - (score_array.size() - i) * 8 * 4),
                10,
                32,
                40
            };

            score_dest_rect.push_back(dest);
        }
    
        /**
         * User Interface
         */

        int mouse_x, mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);

        SDL_RenderClear(renderer);

        for (int i = 0; i < (SCREEN_WIDTH / 143); i++)
            sp::render_texture(renderer, tex, background_rects[i], &background);

        sp::render_texture(renderer, ground_texture, (int)ground_x_1, SCREEN_HEIGHT - 60);
        sp::render_texture(renderer, ground_texture, (int)ground_x_2, SCREEN_HEIGHT - 60);

        for (std::vector<Obstacle>::iterator it = obstacles.begin(); it != obstacles.end(); it++)
            it->draw(renderer);

        if (!player.is_dead())
            for (int i = 0; i < score_dest_rect.size(); i++) {
                sp::render_texture(renderer, tex, score_dest_rect[i], &numbers[score_array[i]]);
            }

        player.draw(renderer);

        // sp::render_texture(renderer, tex, start_dest, &start_btn);

        if (player.is_dead()) {

            high_score_list.push_back(player.get_score());

            if (high_score_list.size()) {
                best_score = *std::max_element(high_score_list.begin(),
                        high_score_list.end());
            };

            if (!set_best_score) {
                high_score_fs << player.get_score() << std::endl;
                set_best_score = true;
            }

            if (CollisionBank::check_point_collision(mouse_x, mouse_y, ok_dest)) {
                ok_hot = true;
            } else {
                ok_hot = false;
                ok_active = false;
            }

            if(ok_hot) {
                if (!ok_active && mouse_down) {
                    ok_active = true;
                    ok_dest.y += 5;
                }
                else if (ok_active && !mouse_down) {
                    // reset the game
                    player.set_alive();
                    player.set_idle();
                    player.set_y(SCREEN_HEIGHT / 2 - 60);
                    set_best_score = false;

                    for (int i = 0; i < obstacles.size(); i++)
                        obstacles[i].set_x(600 + 200 * i);

                    ok_active = false;
                    ok_dest.y -= 5;
                }
            }

            sp::render_texture(renderer, tex, game_over_dest, &game_over_src);
            sp::render_texture(renderer, tex, score_board_dest, &score_board_src);
            sp::render_texture(renderer, tex, ok_dest, &ok_src);

            std::vector<SDL_Rect> tmp_score_dest_rect;
            std::vector<SDL_Rect> tmp_best_score_dest_rect;
            std::vector<int> high_score = digit_to_array(player.get_score());
            std::vector<int> best_score_vec = digit_to_array(best_score);

            for (int i = high_score.size() - 1; i >= 0; i--) {
                SDL_Rect dest = {
                    85 + (int)(SCREEN_WIDTH / 2) - (12 * i + 10),
                    (int)(SCREEN_HEIGHT / 2),
                    12,
                    14
                };

                tmp_score_dest_rect.push_back(dest);
            }

            for (int i = best_score_vec.size() - 1; i >= 0; i--) {
                SDL_Rect dest = {
                    85 + (int)(SCREEN_WIDTH / 2) - (12 * i + 10),
                    50 + (int)(SCREEN_HEIGHT / 2),
                    12,
                    14
                };

                tmp_best_score_dest_rect.push_back(dest);
            }

            for (int i = 0; i < tmp_score_dest_rect.size(); i++) {
                sp::render_texture(renderer, tex, tmp_score_dest_rect[i], &numbers[high_score[i] + 10]);
            }

            for (int i = 0; i < best_score_vec.size(); i++) {
                sp::render_texture(renderer, tex, tmp_best_score_dest_rect[i], &numbers[best_score_vec[i] + 10]);
            }
        }

        if (player.is_idle()) {
            tap_dest.y = SCREEN_HEIGHT / 2 + 5 * cos(last_tick / 120.0f);

            sp::render_texture(renderer, tex, tap_dest, &tap_src);
        }

        SDL_RenderPresent(renderer);
    }

    while(Mix_Playing(-1) != 0);

    high_score_fs.close();
    g_score = nullptr;

    Mix_FreeChunk(g_score);

    SDL_DestroyTexture(tex);
    SDL_DestroyTexture(ground_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    Mix_Quit();
    SDL_Quit();

    return 0;
}
