#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define FR(i,a,b) for(int i=(a);i<(b);++i)
#define FOR(i,n) FR(i,0,n)
#define BEND(v) (v).begin(),(v).end()

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
using sdl_ptr = std::unique_ptr<T, delete_sdl>;

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

void cleanup()
{
    tile_floor.reset();

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

// Hex cube coordinates:
// - down-right: +S
// - up: +T
// - down-left: +P
int player_s, player_t;

void move_player(int ds, int dt)
{
    player_s += ds;
    player_t += dt;
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

            // Movement:
            //  i o
            // j   l
            //  m ,
            if (e.key.keysym.sym == SDLK_i) {
                move_player(-1, 1);
            }
            if (e.key.keysym.sym == SDLK_o) {
                move_player(0, 1);
            }
            if (e.key.keysym.sym == SDLK_j) {
                move_player(-1, 0);
            }
            if (e.key.keysym.sym == SDLK_l) {
                move_player(1, 0);
            }
            if (e.key.keysym.sym == SDLK_m) {
                move_player(0, -1);
            }
            if (e.key.keysym.sym == SDLK_COMMA) {
                move_player(1, -1);
            }
        }
    }
}

int const HORIZONTAL_HALF_PERIOD_PX = 37;
int const VERTICAL_HALF_PERIOD_PX = 60;

void hex_to_pixel(int s, int t, int * x_px, int * y_px)
{
    int p = -s-t;

    int origin_x_px = WIN_WIDTH/2;
    int origin_y_px = WIN_HEIGHT/2;

    *x_px = origin_x_px + HORIZONTAL_HALF_PERIOD_PX * (s - p);
    *y_px = origin_y_px - VERTICAL_HALF_PERIOD_PX * t;
}

void render()
{
    CHECK_SDL(SDL_SetRenderDrawColor(ren, 0, 0, 0, 255));
    CHECK_SDL(SDL_RenderClear(ren));

    //// draw floor
    FOR(row, 5) {
        int s_lo = std::max(-row, -2);
        int s_hi = std::min(2, 4-row) + 1;

        int t = row-2;

        FR(s, s_lo, s_hi) {
            int x_px, y_px;
            hex_to_pixel(s, t, &x_px, &y_px);

            SDL_Rect dstrect = { x_px - tile_floor_w/2, y_px - tile_floor_h/2, tile_floor_w, tile_floor_h };
            SDL_RenderCopy(ren, tile_floor.get(), NULL, &dstrect);
        }
    }

    //// draw player
    int player_x_px, player_y_px;
    hex_to_pixel(player_s, player_t, &player_x_px, &player_y_px);
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

    // init game
    player_s = 0;
    player_t = 0;

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
