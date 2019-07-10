#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <map>
#include <vector>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// https://github.com/nlohmann/json
#include "nlohmann/json.hpp"

#define FR(i,a,b) for(int i=(a);i<(b);++i)
#define FOR(i,n) FR(i,0,n)
#define BEND(v) (v).begin(),(v).end()

using std::make_pair;
using std::unique_ptr;
using nlohmann::json;

// SDL utilities
struct delete_sdl
{
    void operator()(SDL_Texture * p) const
    {
        SDL_DestroyTexture(p);
    }

    void operator()(SDL_Surface * p) const
    {
        SDL_FreeSurface(p);
    }
};

template<class T>
using sdl_ptr = unique_ptr<T, delete_sdl>;

void failSDL(const char * msg)
{
    std::printf("SDL %s failed: %s\n", msg, SDL_GetError());
    exit(1);
}
#define CHECK_SDL(expr) if ((expr) < 0) failSDL(#expr)

void failTTF(const char * msg)
{
    std::printf("TTF %s failed: %s\n", msg, TTF_GetError());
    exit(1);
}
#define CHECK_TTF(expr) if ((expr) < 0) failTTF(#expr)

void failIMG(const char * msg)
{
    std::printf("IMG %s failed: %s\n", msg, IMG_GetError());
    exit(1);
}
#define CHECK_IMG(expr) if ((expr) < 0) failIMG(#expr)

int const TEXT_ALIGNH_LEFT = 0;
int const TEXT_ALIGNH_CENTER = 1;
int const TEXT_ALIGNH_RIGHT = 2;

void DrawText(SDL_Renderer * ren, TTF_Font * font, const char * s, SDL_Color color, int x, int y, int * textW, int * textH, int alignh = TEXT_ALIGNH_LEFT)
{
    int tW, tH;
    if (textW == NULL) textW = &tW;
    if (textH == NULL) textH = &tH;

    sdl_ptr<SDL_Surface> textSurf(TTF_RenderText_Solid(font, s, color));
    if (!textSurf) failTTF("TTF_RenderText_Solid");

    sdl_ptr<SDL_Texture> textTex(SDL_CreateTextureFromSurface(ren, textSurf.get()));
    if (!textTex) failSDL("SDL_CreateTextureFromSurface");

    if (SDL_QueryTexture(textTex.get(), NULL, NULL, textW, textH) < 0) failSDL("SDL_QueryTexture");

    if (alignh == TEXT_ALIGNH_CENTER) {
        x -= *textW / 2;
    } else if (alignh == TEXT_ALIGNH_RIGHT) {
        x -= *textW;
    }

    SDL_Rect dst = { x, y, *textW, *textH };
    if (SDL_RenderCopy(ren, textTex.get(), NULL, &dst) < 0) failSDL("SDL_RenderCopy");
}

SDL_Texture * LoadTexture(SDL_Renderer * ren, const char * path)
{
    SDL_Texture *tex = IMG_LoadTexture(ren, path);
    if (!tex) failIMG("LoadTexture");
    return tex;
}

// SDL data, cleanup, etc.
SDL_Window * win = NULL;
TTF_Font * font = NULL;
SDL_Renderer * ren = NULL;

sdl_ptr<SDL_Texture> tile_floor;
int tile_floor_w;
int tile_floor_h;
sdl_ptr<SDL_Texture> tile_wall;

struct Sprite
{
    sdl_ptr<SDL_Texture> tex;
    int w, h;
};

std::map<std::string, unique_ptr<Sprite>> sprites;

void LoadSprite(const char * path)
{
    unique_ptr<Sprite> s(new Sprite);
    s->tex.reset(LoadTexture(ren, path));
    CHECK_SDL(SDL_QueryTexture(s->tex.get(), NULL, NULL, &s->w, &s->h));
    sprites[path] = std::move(s);
}

void cleanup()
{
    tile_floor.reset();
    tile_wall.reset();
    sprites.clear();

    if (ren) SDL_DestroyRenderer(ren);
    if (font) TTF_CloseFont(font);
    if (win) SDL_DestroyWindow(win);

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

// FPS tracking
const int CIRCBUF_LEN = 64;
Uint32 circbuf[CIRCBUF_LEN];
int circbuf_i;

void initFPSTracking()
{
    memset(circbuf, 0, sizeof(circbuf));
    circbuf_i = 0;
}

void accumTime(Uint32 ms)
{
    circbuf[circbuf_i++] = ms;
    if (circbuf_i >= CIRCBUF_LEN) circbuf_i = 0;
}

double avgFrameTime_ms()
{
    double ret = 0;
    FOR(i, CIRCBUF_LEN) ret += circbuf[i];
    return ret / CIRCBUF_LEN;
}

// main code
const int WIN_WIDTH = 1600;
const int WIN_HEIGHT = 1200;

const int FONT_HEIGHT = 16;

int const HORIZONTAL_HALF_PERIOD_PX = 37;
int const VERTICAL_HALF_PERIOD_PX = 60;

int const ORIGIN_X_PX = WIN_WIDTH/2;
int const ORIGIN_Y_PX = WIN_HEIGHT/2;

// Hex cube coordinates:
// - down-right: +S
// - up: +T
// - down-left: +P
int const NDIRS = 6;
int const DIR_DS[NDIRS] = {  1,  0, -1, -1,  0,  1 };
int const DIR_DT[NDIRS] = {  0,  1,  1,  0, -1, -1 };

void hex_to_pixel(int s, int t, int * x_px, int * y_px)
{
    int p = -s-t;

    *x_px = ORIGIN_X_PX + HORIZONTAL_HALF_PERIOD_PX * (s - p);
    *y_px = ORIGIN_Y_PX - VERTICAL_HALF_PERIOD_PX * t;
}

int camera_x_px;
int camera_y_px;

void hex_to_screen(int s, int t, int * x_scr, int * y_scr)
{
    int x_px, y_px;
    hex_to_pixel(s, t, &x_px, &y_px);
    *x_scr = x_px - camera_x_px;
    *y_scr = y_px - camera_y_px;
}

double const CAMERA_TWEEN_SPEED = 10.0;

int player_s, player_t;

enum class TileType
{
    none,
    floor,
    wall
};

std::map<std::pair<int,int>, TileType> tiles;

bool is_tile_blocking(int s, int t)
{
    return tiles[make_pair(s, t)] != TileType::floor;
}

enum class EntityType
{
    none,
    blue_bat
};

struct Entity
{
    int s,t;
    EntityType type;

    Sprite * sprite;

    bool is_dead;

    Entity(int s_, int t_, EntityType type_)
        : s(s_), t(t_), type(type_), sprite(NULL), is_dead(false)
    {
        if (type == EntityType::blue_bat) {
            sprite = sprites.at("data/blue_bat.png").get();
        } else {
            assert(!"No sprite for entity type");
        }
    }

    void move();

    void be_hit()
    {
        is_dead = true;
    }

    void render()
    {
        if (is_dead) return;

        int x_px, y_px;
        hex_to_screen(s, t, &x_px, &y_px);

        SDL_Rect dstrect = { x_px - sprite->w/2, y_px - sprite->h/2, sprite->w, sprite->h };
        SDL_RenderCopy(ren, sprite->tex.get(), NULL, &dstrect);
    }
};

std::vector<Entity> entities;

void player_be_hit()
{
    fprintf(stderr, "player was hit\n");
}

void Entity::move()
{
    if (is_dead) return;

    int num_open_dirs = 0;
    int open_dirs[NDIRS];

    FOR(d,NDIRS) {
        int target_s = s + DIR_DS[d];
        int target_t = t + DIR_DT[d];

        if (!is_tile_blocking(target_s, target_t)) {
            open_dirs[num_open_dirs++] = d;
        }
    }

    if (num_open_dirs == 0) return;

    int i = rand() % num_open_dirs;
    int d = open_dirs[i];
    int target_s = s + DIR_DS[d];
    int target_t = t + DIR_DT[d];

    bool cancel_move = false;
    for (auto& e : entities) {
        if (!e.is_dead && e.s == target_s && e.t == target_t) {
            cancel_move = true;
        }
    }

    if (player_s == target_s && player_t == target_t) {
        cancel_move = true;
        player_be_hit();
    }

    if (!cancel_move) {
        s = target_s;
        t = target_t;
    }
}

void move_player(int dir)
{
    int target_s = player_s + DIR_DS[dir];
    int target_t = player_t + DIR_DT[dir];

    if (is_tile_blocking(target_s, target_t)) return;

    bool did_attack = false;
    for (auto& e : entities) {
        //fprintf(stderr, "%d %d : %d %d\n", target_s, target_t, e.s, e.t);
        if (e.s == target_s && e.t == target_t && !e.is_dead) {
            did_attack = true;
            e.be_hit();
        }
    }

    if (!did_attack) {
        player_s = target_s;
        player_t = target_t;
    }

    //// enemy movement
    for (auto& t : entities) {
        t.move();
    }
}

void load_map()
{
    // load map
    json j;
    std::ifstream i("data/map.json");
    i >> j;
    i.close();

    player_s = j["player_s"].get<int>();
    player_t = j["player_t"].get<int>();

    tiles.clear();
    for (auto& rec : j["tiles"]) {
        int s = rec["s"].get<int>();
        int t = rec["t"].get<int>();
        std::string type = rec["type"].get<std::string>();

        TileType tile_type = TileType::none;
        if (type == "wall") {
            tile_type = TileType::wall;
        } else if (type == "floor") {
            tile_type = TileType::floor;
        } else {
            assert(!"Unrecognized tile type");
        }

        tiles[make_pair(s,t)] = tile_type;
    }

    auto e_json = j.find("entities");
    assert(e_json != j.end());
    entities.clear();
    for (auto& rec : *e_json) {
        int s = rec["s"].get<int>();
        int t = rec["t"].get<int>();
        std::string type = rec["type"].get<std::string>();

        EntityType entity_type = EntityType::none;

        if (type == "enemy_blue_bat") {
            entity_type = EntityType::blue_bat;
        } else {
            assert(!"Unrecognized entity type");
        }

        entities.push_back(Entity(s, t, entity_type));
    }
}

void snap_camera_to_player()
{
    int player_x_px, player_y_px;
    hex_to_pixel(player_s, player_t, &player_x_px, &player_y_px);

    camera_x_px = player_x_px - ORIGIN_X_PX;
    camera_y_px = player_y_px - ORIGIN_Y_PX;
}

void reset_game()
{
    load_map();
    snap_camera_to_player();
}

double deltaFrame_s;

bool quitRequested;
void update()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            quitRequested = true;
        }

        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                quitRequested = true;
            }
            if (e.key.keysym.sym == SDLK_BACKSPACE) {
                reset_game();
            }

            // Movement:
            //  i o
            // j   l
            //  m ,
            if (e.key.keysym.sym == SDLK_l) {
                move_player(0);
            }
            if (e.key.keysym.sym == SDLK_o) {
                move_player(1);
            }
            if (e.key.keysym.sym == SDLK_i) {
                move_player(2);
            }
            if (e.key.keysym.sym == SDLK_j) {
                move_player(3);
            }
            if (e.key.keysym.sym == SDLK_m) {
                move_player(4);
            }
            if (e.key.keysym.sym == SDLK_COMMA) {
                move_player(5);
            }
        }
    }
}

void render()
{
    //// update camera
    {
        int player_x_px, player_y_px;
        hex_to_pixel(player_s, player_t, &player_x_px, &player_y_px);

        int target_x_px = player_x_px - ORIGIN_X_PX;
        int target_y_px = player_y_px - ORIGIN_Y_PX;

        double alpha = exp(-deltaFrame_s * CAMERA_TWEEN_SPEED);
        camera_x_px = static_cast<int>(round(alpha * camera_x_px + (1-alpha) * target_x_px));
        camera_y_px = static_cast<int>(round(alpha * camera_y_px + (1-alpha) * target_y_px));
    }

    //// clear screen
    CHECK_SDL(SDL_SetRenderDrawColor(ren, 0, 0, 0, 255));
    CHECK_SDL(SDL_RenderClear(ren));

    //// draw tiles
    for (auto& it : tiles) {
        int s = it.first.first;
        int t = it.first.second;
        TileType type = it.second;

        SDL_Texture * tex = NULL;
        switch (type) {
        case TileType::floor: tex = tile_floor.get(); break;
        case TileType::wall: tex = tile_wall.get(); break;
        case TileType::none: assert(!"Render encountered TileType::none"); break;
        }

        int x_px, y_px;
        hex_to_screen(s, t, &x_px, &y_px);

        SDL_Rect dstrect = { x_px - tile_floor_w/2, y_px - tile_floor_h/2, tile_floor_w, tile_floor_h };
        SDL_RenderCopy(ren, tex, NULL, &dstrect);
    }

    //// draw entities
    for (auto& e : entities) {
        e.render();
    }

    //// draw player
    int player_x_px, player_y_px;
    hex_to_screen(player_s, player_t, &player_x_px, &player_y_px);
    int player_w_px=64, player_h_px=64;
    CHECK_SDL(SDL_SetRenderDrawColor(ren, 255, 255, 255, 255));
    SDL_Rect rect = { player_x_px - player_w_px/2, player_y_px - player_h_px/2, player_w_px, player_h_px };
    CHECK_SDL(SDL_RenderFillRect(ren, &rect));

    //// diagnostics
    CHECK_SDL(SDL_SetRenderDrawColor(ren, 255, 255, 255, 255));
    char buf[256];

    snprintf(buf, sizeof(buf), "S=%2d T=%2d", player_s, player_t);
    DrawText(ren, font, buf, {255, 255, 255, 255}, 0, 0, NULL, NULL, TEXT_ALIGNH_LEFT);

    snprintf(buf, sizeof(buf), "t=%.1lf ms", avgFrameTime_ms());
    DrawText(ren, font, buf, {255, 255, 255, 255}, WIN_WIDTH, 0, NULL, NULL, TEXT_ALIGNH_RIGHT);

    SDL_RenderPresent(ren);
}

Uint32 prevFrame_ms;
void main_loop()
{
    Uint32 thisFrame_ms = SDL_GetTicks();
    Uint32 deltaFrame_ms = thisFrame_ms - prevFrame_ms;
    accumTime(deltaFrame_ms);
    deltaFrame_s = deltaFrame_ms / 1000.0;
    update();
    render();

    prevFrame_ms = thisFrame_ms;
}

int main()
{
    srand(time(NULL));
    atexit(cleanup);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) failSDL("SDL_Init");
    if (TTF_Init() == -1) failTTF("TTF_Init");

    int flags = IMG_INIT_PNG;
    if ((IMG_Init(flags) & flags) != flags) failIMG("IMG_Init");

    font = TTF_OpenFont("data/Vera.ttf", FONT_HEIGHT);
    if (!font) failTTF("TTF_OpenFont");

    win = SDL_CreateWindow("Hex Dance Dungeon",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!win) failSDL("SDL_CreateWindow");

    ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) failSDL("SDL_CreateRenderer");

    // load textures
    tile_floor.reset(LoadTexture(ren, "data/tile_floor.png"));
    CHECK_SDL(SDL_QueryTexture(tile_floor.get(), NULL, NULL, &tile_floor_w, &tile_floor_h));
    tile_wall.reset(LoadTexture(ren, "data/tile_wall.png"));
    LoadSprite("data/blue_bat.png");

    // init game
    reset_game();

    // IO loop
    prevFrame_ms = SDL_GetTicks();
    quitRequested = false;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 0);
#else
    while (!quitRequested) {
        main_loop();
    }
#endif

    return 0;
}
