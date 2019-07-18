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

#include "hex_dance_dungeon.hpp"

using std::make_pair;
using std::unique_ptr;
using nlohmann::json;
using std::make_tuple;

// Globals
std::map<std::pair<int,int>, TileType> tiles;
std::set<std::tuple<int,int>> is_visible;
std::set<std::tuple<int,int>> tile_has_been_visible;
int player_s, player_t;

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

void LoadSprite(const char * path, int nframes = 1)
{
    unique_ptr<Sprite> s(new Sprite);
    s->tex.reset(LoadTexture(ren, path));
    CHECK_SDL(SDL_QueryTexture(s->tex.get(), NULL, NULL, &s->w, &s->h));
    s->w /= nframes;
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

double deltaFrame_s;

// Many of the hex grid routines are informed by
// https://www.redblobgames.com/grids/hexagons
//
// Hex cube coordinates:
// - down-right: +S
// - up: +T
// - down-left: +P
int const NDIRS = 6;
int const DIR_DS[NDIRS] = {  1,  0, -1, -1,  0,  1 };
int const DIR_DT[NDIRS] = {  0,  1,  1,  0, -1, -1 };

int positive_mod(int x, int m)
{
    return (x % m + m) % m;
}

// Means: how many 60-degree increments separate these two directions.
int dir_deviation(int d1, int d2)
{
    return std::min(positive_mod(d2-d1, NDIRS), positive_mod(d1-d2, NDIRS));
}

int hex_dist(int s1, int t1, int s2, int t2)
{
    int p1 = -s1-t1;
    int p2 = -s2-t2;

    return (abs(s1-s2) + abs(t1-t2) + abs(p1-p2))/2;
}

int hex_dist_l2sq(int s1, int t1, int s2, int t2)
{
    // This formula assumes that the center-to-center distance of adjacent hexes is 1.
    int ds = s2-s1, dt = t2-t1;
    return ds*ds + dt*dt + ds*dt;
}

std::tuple<int, int> hex_to_pixel(int s, int t)
{
    int p = -s-t;

    return make_tuple(
            ORIGIN_X_PX + HORIZONTAL_HALF_PERIOD_PX * (s - p),
            ORIGIN_Y_PX - VERTICAL_HALF_PERIOD_PX * t);
}

int camera_x_px;
int camera_y_px;

std::tuple<int, int> pixel_to_screen(std::tuple<int, int> pos)
{
    auto [ x_px, y_px ] = pos;
    return make_tuple(x_px - camera_x_px, y_px - camera_y_px);
}

std::tuple<int, int> hex_to_screen(int s, int t)
{
    return pixel_to_screen(hex_to_pixel(s, t));
}

double const CAMERA_TWEEN_SPEED = 10.0;

// Where was the player at the start of the current turn?
int player_prev_s, player_prev_t;

bool is_tile_blocking(int s, int t)
{
    auto i = tiles.find(make_pair(s,t));
    if (i == tiles.end()) return false;
    return i->second != TileType::floor;
}

void compute_visibility_plus()
{
    compute_visibility();
    for (auto& h : is_visible) {
        tile_has_been_visible.insert(h);
    }
}

void player_be_hit()
{
    fprintf(stderr, "player was hit\n");
}

Sprite * telegraph_arrows[6];

enum class EntityType
{
    none,
    bat_blue,
    slime_blue,
    skeleton_white
};

enum class TweenType
{
    none,
    move,
    bump
};

double const TWEEN_MOVE_LEN_S = 0.08;
double const TWEEN_BUMP_LEN_S = 0.08;

struct Tweener
{
    TweenType type = TweenType::none;
    int src_x_px=0, src_y_px=0, dst_x_px=0, dst_y_px=0;
    double t = 0.0;

    void ease_move_px(std::tuple<int,int> src_px, std::tuple<int, int> dst_px)
    {
        type = TweenType::move;
        std::tie(src_x_px, src_y_px) = src_px;
        std::tie(dst_x_px, dst_y_px) = dst_px;
        t = 0;
    }

    void ease_bump_px(std::tuple<int, int> dst_px, std::tuple<int, int> bumped_px)
    {
        type = TweenType::bump;
        std::tie(src_x_px, src_y_px) = bumped_px;
        std::tie(dst_x_px, dst_y_px) = dst_px;
        t = 0;
    }

    void set_pos_px(std::tuple<int, int> pos_px)
    {
        type = TweenType::none;
        std::tie(dst_x_px, dst_y_px) = pos_px;
        t = 0;
    }

    std::tuple<int, int> get_pos_px()
    {
        if (type == TweenType::none) return make_tuple(dst_x_px, dst_y_px);

        double tween_len_s = 0.0;
        if (type == TweenType::move) tween_len_s = TWEEN_MOVE_LEN_S;
        if (type == TweenType::bump) tween_len_s = TWEEN_BUMP_LEN_S;

        t += deltaFrame_s;
        if (t > tween_len_s) {
            type = TweenType::none;
            return make_tuple(dst_x_px, dst_y_px);
        }
        double pct = (tween_len_s - t) / tween_len_s;
        int x_px = dst_x_px, y_px = dst_y_px;
        double alpha = (1 - cos(pct * M_PI / 2));

        if (type == TweenType::bump) {
            if (alpha > 0.5) alpha = 1.0 - alpha;
            alpha *= 0.5;
        }

        x_px += static_cast<int>(round((src_x_px - x_px) * alpha));
        y_px += static_cast<int>(round((src_y_px - y_px) * alpha));

        return make_tuple(x_px, y_px);
    }
};

struct Entity
{
    int s=0,t=0;
    EntityType type = EntityType::none;

    Sprite * sprite = NULL;
    Tweener tweener;
    bool is_dead = false;
    bool has_been_visible = false;

    // bat_blue, slime_blue
    int prep_dir = -1;

    // slime_blue, skeleton_white
    int moveCooldown = 1;

    // slime_blue
    int parity = 0;

    // skeleton_white
    int momentum_dir = 3;

    bool is_inactive()
    {
        return is_dead || !has_been_visible;
    }

    void move()
    {
        if (is_inactive()) return;

        int move_dir = -1;

        if (type == EntityType::bat_blue || type == EntityType::slime_blue) {
            move_dir = prep_dir;
            prep_dir = -1;
        } else if (type == EntityType::skeleton_white) {
            if (moveCooldown > 0) {
                --moveCooldown;
                return;
            }

            // If we can't get closer to the player's current or previous position, prefer standing still.
            auto best_key = make_tuple(
                    hex_dist(s, t, player_s, player_t),
                    hex_dist(s, t, player_prev_s, player_prev_t),
                    0);

            // If all our desired moves are blocked, then instead of standing still,
            // bump whichever tile we'd most like to be empty.
            auto bump_key = best_key;
            int bump_dir = -1;

            FOR(d,NDIRS) {
                int new_s = s + DIR_DS[d];
                int new_t = t + DIR_DT[d];

                // Always hit player when possible
                if (player_s == new_s && player_t == new_t) {
                    move_dir = d;
                    break;
                }

                auto cur_key = make_tuple(
                        hex_dist(new_s, new_t, player_s, player_t),
                        hex_dist(new_s, new_t, player_prev_s, player_prev_t),
                        dir_deviation(momentum_dir, d));

                if (cur_key < bump_key) {
                    bump_key = cur_key;
                    bump_dir = d;
                }

                if (is_tile_blocking(new_s, new_t) || Entity::is_at(new_s, new_t)) continue;

                if (cur_key < best_key) {
                    best_key = cur_key;
                    move_dir = d;
                }
            }

            if (move_dir == -1) move_dir = bump_dir;
        }

        if (move_dir == -1) return;

        momentum_dir = move_dir;

        // We're committed to trying to move
        if (type == EntityType::slime_blue || type == EntityType::skeleton_white) {
            moveCooldown = 1;
        }

        int target_s = s + DIR_DS[move_dir];
        int target_t = t + DIR_DT[move_dir];

        if (is_tile_blocking(target_s, target_t) || Entity::is_at(target_s, target_t)) {
            tweener.ease_bump_px(hex_to_pixel(s, t), hex_to_pixel(target_s, target_t));
        } else if (player_s == target_s && player_t == target_t) {
            tweener.ease_bump_px(hex_to_pixel(s, t), hex_to_pixel(target_s, target_t));
            player_be_hit();
        } else {
            tweener.ease_move_px(hex_to_pixel(s, t), hex_to_pixel(target_s, target_t));
            s = target_s;
            t = target_t;
        }
    }

    void think()
    {
        if (is_inactive()) return;

        if (type == EntityType::bat_blue) {
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
            prep_dir = open_dirs[i];
        } else if (type == EntityType::slime_blue) {
            if (moveCooldown > 0) {
                --moveCooldown;
                return;
            }

            prep_dir = 3 * parity;
            parity = (parity+1)%2;
        }
    }

    void be_hit()
    {
        is_dead = true;
    }

    void render()
    {
        if (is_dead) return;

        // main sprite
        auto [ x_px, y_px ] = pixel_to_screen(tweener.get_pos_px());

        if (tile_has_been_visible.find(make_tuple(s,t)) == tile_has_been_visible.end()) return;

        int frame = 0;

        if (type == EntityType::skeleton_white) {
            if (moveCooldown == 0) {
                frame = 1;
            }
        }

        SDL_Rect srcrect = { frame * sprite->w, 0, sprite->w, sprite->h };
        SDL_Rect dstrect = { x_px - sprite->w/2, y_px - sprite->h/2, sprite->w, sprite->h };
        SDL_RenderCopy(ren, sprite->tex.get(), &srcrect, &dstrect);

        // telegraph arrow
        int tile_x_px = x_px - tile_floor_w/2;
        int tile_y_px = y_px - tile_floor_h/2;

        if (prep_dir != -1) {
            assert(0 <= prep_dir && prep_dir < NDIRS);
            int xoff = 0, yoff = 0;

            switch (prep_dir) {
            case 0: xoff = 70; yoff = 34; break;
            case 1: xoff = 52; yoff =  3; break;
            case 2: xoff = 14; yoff =  2; break;
            case 3: xoff = -6; yoff = 34; break;
            case 4: xoff = 15; yoff = 65; break;
            case 5: xoff = 52; yoff = 64; break;
            }

            dstrect = { tile_x_px + xoff, tile_y_px + yoff, telegraph_arrows[prep_dir]->w, telegraph_arrows[prep_dir]->h };
            SDL_RenderCopy(ren, telegraph_arrows[prep_dir]->tex.get(), NULL, &dstrect);
        }
    }

    void init()
    {
        switch (type) {
        case EntityType::bat_blue: sprite = sprites.at("data/bat_blue.png").get(); break;
        case EntityType::slime_blue: sprite = sprites.at("data/slime_blue.png").get(); break;
        case EntityType::skeleton_white: sprite = sprites.at("data/skeleton_white.png").get(); break;
        default: assert(!"Unrecognized entity type");
        }

        tweener.set_pos_px(hex_to_pixel(s, t));
    }

    std::tuple<int, int, int>
    priority_key()
    {
        return make_tuple(
            hex_dist_l2sq(s, t, player_s, player_t),
            t,
            s);
    }

    static void load_textures()
    {
        LoadSprite("data/bat_blue.png");
        LoadSprite("data/slime_blue.png");
        LoadSprite("data/skeleton_white.png", 2);

        FOR(d,NDIRS) {
            std::string path = "data/telegraph_arrow_";
            path.push_back('0' + d);
            path += ".png";

            LoadSprite(path.c_str());
            telegraph_arrows[d] = sprites[path].get();
        }
    }

    static std::vector<unique_ptr<Entity>> entities;

    static std::vector<Entity*> prioritized;

    static void move_enemies()
    {
        prioritized.clear();
        for (auto& e : entities) {
            prioritized.push_back(e.get());
        }
        sort(BEND(prioritized), [](Entity * e1, Entity * e2) {
            return e1->priority_key() < e2->priority_key();
        });

        for (auto& e : prioritized) {
            e->move();
        }
        for (auto& e : prioritized) {
            e->think();
        }

        // Wake visible enemies AFTER movement,
        // so that they don't start moving instantly when seen.
        for (auto& e : entities) {
            if (!e->has_been_visible && is_visible.find(make_tuple(e->s, e->t)) != is_visible.end()) {
                e->has_been_visible = true;
            }
        }
    }

    static void render_enemies()
    {
        for (auto& e : entities) {
            e->render();
        }
    }

    static Entity * get_at(int s, int t)
    {
        for (auto& e : entities) {
            if (!e->is_dead && e->s == s && e->t == t) {
                return e.get();
            }
        }
        return NULL;
    }

    static bool is_at(int s, int t)
    {
        return get_at(s, t) != NULL;
    }
};

std::vector<unique_ptr<Entity>> Entity::entities;
std::vector<Entity*> Entity::prioritized;

void move_player(int dir)
{
    player_prev_s = player_s;
    player_prev_t = player_t;

    int ds = 0, dt = 0;
    if (dir != -1) {
        ds = DIR_DS[dir];
        dt = DIR_DT[dir];
    }
    int target_s = player_s + ds;
    int target_t = player_t + dt;

    if (is_tile_blocking(target_s, target_t)) return;

    bool did_attack = false;
    Entity * e = Entity::get_at(target_s, target_t);
    if (e) {
        did_attack = true;
        e->be_hit();
    }

    if (!did_attack) {
        player_s = target_s;
        player_t = target_t;
    }

    compute_visibility_plus();

    Entity::move_enemies();
}

struct MapBuilder
{
    int player_s=0, player_t=0;
    json tiles = json::array();
    json entities = json::array();

    void player(int s, int t)
    {
        player_s = s;
        player_t = t;
    }

    void tile(int s, int t, const char * type)
    {
        tiles.push_back({ { "s", s }, { "t", t }, { "type", type } });
    }

    void entity(int s, int t, const char * type)
    {
        entities.push_back({ { "s", s }, { "t", t }, { "type", type } });
    }

    void hex_room(int min_s, int min_t, int s_len, int t_len, int trim_min, int trim_max)
    {
        int max_s = min_s + s_len;
        int max_t = min_t + t_len;

        FR(s, min_s, max_s+1) {
            FR(t, min_t, max_t+1) {
                int slack_min = (s - min_s + t - min_t) - trim_min;
                int slack_max = (max_s - s + max_t - t) - trim_max;

                if (slack_min < 0 || slack_max < 0) continue;

                const char * type = "wall";
                if (min_s < s && s < max_s && min_t < t && t < max_t && slack_min > 0 && slack_max > 0) {
                    type = "floor";
                }

                tile(s, t, type);
            }
        }
    }

    json make_json()
    {
        json j;

        j["player_s"] = player_s;
        j["player_t"] = player_t;
        j["tiles"] = tiles;
        j["entities"] = entities;

        return j;
    }
};

json random_map_json()
{
    MapBuilder b;

    b.hex_room(0, -6, 7, 6, 3, 3);
    b.hex_room(3, -12, 7, 6, 3, 3);

    b.hex_room(4, -3, 7, 6, 3, 3);
    b.hex_room(7, -9, 7, 6, 3, 3);
    b.hex_room(10, -15, 7, 6, 3, 3);

    b.hex_room(11, -6, 7, 6, 3, 3);
    b.hex_room(14, -12, 7, 6, 3, 3);

    b.tile(5, -6, "floor");
    b.tile(7, -5, "floor");
    b.tile(5, -1, "floor");

    b.tile(10, -10, "floor");
    b.tile(12, -9, "floor");
    b.tile(11, -2, "floor");
    b.tile(12, -4, "floor");

    b.tile(15, -10, "floor");
    b.tile(16, -6, "floor");

    b.player(3, -3);

    int const NROOM = 6;

    std::vector<const char *> supplement = {
        "enemy_skeleton_white",
        "enemy_skeleton_white",
        "enemy_slime_blue",
        "enemy_slime_blue",
        "enemy_bat_blue",
        "enemy_bat_blue",
        };

    std::random_shuffle(BEND(supplement));

    int s0[NROOM] = { 3, 4, 7, 10, 11, 14 };
    int t0[NROOM] = { -12, -3, -9, -15, -6, -12 };

    FOR(i,NROOM) {
        std::vector<const char *> contents = {
            "enemy_skeleton_white",
            "enemy_slime_blue",
            "enemy_bat_blue",
            supplement[i],
            };
        random_shuffle(BEND(contents));

        b.entity(s0[i]+3, t0[i]+2, contents[0]);
        b.entity(s0[i]+5, t0[i]+2, contents[1]);
        b.entity(s0[i]+2, t0[i]+4, contents[2]);
        b.entity(s0[i]+4, t0[i]+4, contents[3]);
    }

    return b.make_json();
}

std::string current_map_path;

void load_map()
{
    json j;
    if (current_map_path == "random") {
        j = random_map_json();
    } else {
        std::ifstream i(current_map_path);
        i >> j;
    }

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
    Entity::entities.clear();
    for (auto& rec : *e_json) {
        unique_ptr<Entity> e(new Entity);
        e->s = rec["s"].get<int>();
        e->t = rec["t"].get<int>();

        std::string type = rec["type"].get<std::string>();
        if (type == "enemy_bat_blue") {
            e->type = EntityType::bat_blue;
        } else if (type == "enemy_slime_blue") {
            e->type = EntityType::slime_blue;
        } else if (type == "enemy_skeleton_white") {
            e->type = EntityType::skeleton_white;
        } else {
            assert(!"Unrecognized entity type");
        }

        e->init();

        Entity::entities.push_back(std::move(e));
    }

    player_s = j["player_s"].get<int>();
    player_t = j["player_t"].get<int>();
}

void snap_camera_to_player()
{
    auto [ player_x_px, player_y_px ] = hex_to_pixel(player_s, player_t);

    camera_x_px = player_x_px - ORIGIN_X_PX;
    camera_y_px = player_y_px - ORIGIN_Y_PX;
}

void reset_game()
{
    load_map();
    player_prev_s = player_s;
    player_prev_t = player_t;
    snap_camera_to_player();

    tile_has_been_visible.clear();
    compute_visibility_plus();
}

void warp_to_map(std::string map_path)
{
    current_map_path = map_path;
    reset_game();
}

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
            // j   ;
            //  k l
            if (e.key.keysym.sym == SDLK_SEMICOLON) {
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
            if (e.key.keysym.sym == SDLK_k) {
                move_player(4);
            }
            if (e.key.keysym.sym == SDLK_l) {
                move_player(5);
            }
            if (e.key.keysym.sym == SDLK_PERIOD) {
                move_player(-1);
            }

            // Maps
            if (e.key.keysym.sym == SDLK_1) {
                warp_to_map("data/map_bat.json");
            } else if (e.key.keysym.sym == SDLK_2) {
                warp_to_map("data/map_slime.json");
            } else if (e.key.keysym.sym == SDLK_3) {
                warp_to_map("data/map_skeleton.json");
            } else if (e.key.keysym.sym == SDLK_4) {
                warp_to_map("data/map_skeleton_line.json");
            } else if (e.key.keysym.sym == SDLK_5) {
                warp_to_map("data/map_proto1.json");
            } else if (e.key.keysym.sym == SDLK_6) {
                warp_to_map("data/map_proto2.json");
            } else if (e.key.keysym.sym == SDLK_7) {
                warp_to_map("random");
            } else if (e.key.keysym.sym == SDLK_0) {
                warp_to_map("data/map_mix.json");
            }
        }
    }
}

void render()
{
    //// update camera
    {
        auto [ player_x_px, player_y_px ] = hex_to_pixel(player_s, player_t);

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

        if (tile_has_been_visible.find(make_tuple(s,t)) == tile_has_been_visible.end()) continue;

        SDL_Texture * tex = NULL;
        switch (type) {
        case TileType::floor: tex = tile_floor.get(); break;
        case TileType::wall: tex = tile_wall.get(); break;
        case TileType::none:
            fprintf(stderr, "Tile at (%d,%d) has type = TileType::none\n", s, t);
            assert(!"Render encountered TileType::none");
            break;
        }

        auto [ x_px, y_px ] = hex_to_screen(s, t);

        SDL_Rect dstrect = { x_px - tile_floor_w/2, y_px - tile_floor_h/2, tile_floor_w, tile_floor_h };
        SDL_RenderCopy(ren, tex, NULL, &dstrect);
    }

    //// draw enemies
    Entity::render_enemies();

    //// draw player
    auto [ player_x_px, player_y_px ] = hex_to_screen(player_s, player_t);
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

    Entity::load_textures();

    // init game
    warp_to_map("data/map_mix.json");

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
