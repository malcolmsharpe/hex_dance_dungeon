#include <vector>

#include "hex_dance_dungeon.hpp"

using std::make_pair;
using std::make_tuple;

struct Slope
{
    int dy=0, dx=1;

    Slope() {}
    Slope(int dy_, int dx_) : dy(dy_), dx(dx_) {}

    bool operator<(Slope const & o) const
    {
        return dy*o.dx < dx*o.dy;
    }
};

static bool operator<=(Slope const & a, Slope const & b)
{
    return !(b < a);
}

static int nrot;
static std::tuple<int,int> st_of_xy(int x, int y)
{
    int s = (2*x-y)/3;
    int t = (2*y-x)/3;
    int p = -s-t;

    FOR(i,nrot) {
        // 60-degree clockwise rotation
        std::tie(s,t,p) = make_tuple(-p,-s,-t);
    }
    
    return make_tuple(player_s+s, player_t+t);
}

static void mark_visible_xy(int x, int y)
{
    auto [ s, t ] = st_of_xy(x, y);

    if (tiles.find(make_pair(s,t)) != tiles.end()) {
        is_visible.insert(make_tuple(s,t));
    }
}

static bool is_tile_opaque_xy(int x, int y)
{
    auto [ s, t ] = st_of_xy(x, y);

    auto it = tiles.find(make_pair(s,t));
    if (it == tiles.end()) return true;
    TileType type = it->second;
    return type != TileType::floor;
}

static std::vector<std::tuple<Slope, Slope>> vis_ivls;
static std::vector<std::tuple<Slope, Slope>> next_vis_ivls;

static void process_one_rot()
{
    vis_ivls.clear();
    vis_ivls.push_back(make_tuple(Slope(0,1), Slope(1,1)));

    for (int x = 2; !vis_ivls.empty(); ++x) {
        next_vis_ivls.clear();

        for (auto [ vis_open, vis_close ] : vis_ivls) {
            int a = vis_open.dy, b = vis_open.dx;
            int k = ((a+b)*x + 2*b) / (3*b);

            Slope next_vis_open = vis_open;

            while (true) {
                int yc = 3*k - x;

                Slope tile_open(yc-1, x), tile_close(yc+1, x);

                if (vis_close <= tile_open) break;

                mark_visible_xy(x, yc);

                if (is_tile_opaque_xy(x, yc)) {
                    if (next_vis_open < tile_open) {
                        next_vis_ivls.push_back(make_tuple(next_vis_open, tile_open));
                    }
                    next_vis_open = tile_close;
                }

                ++k;
            }

            if (next_vis_open < vis_close) {
                next_vis_ivls.push_back(make_tuple(next_vis_open, vis_close));
            }
        }

        std::swap(vis_ivls, next_vis_ivls);
    }
}

void compute_visibility()
{
    is_visible.clear();
    is_visible.insert(make_tuple(player_s,player_t));
    for (nrot = 0; nrot < 6; ++nrot) {
        process_one_rot();
    }
}
