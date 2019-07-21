#include <utility>
#include <vector>

#include "hex_dance_dungeon.hpp"

using std::make_pair;
using std::make_tuple;

struct Slope
{
    int dy=0, dx=1;

    bool operator<(Slope const & o) const
    {
        return dy*o.dx < dx*o.dy;
    }
};

static bool operator<=(Slope const & a, Slope const & b)
{
    return !(b < a);
}

static int origin_s;
static int origin_t;
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
    
    return make_tuple(origin_s+s, origin_t+t);
}

static void mark_visible_xy(int x, int y)
{
    auto [ s, t ] = st_of_xy(x, y);
    mark_tile_visible(s, t);
}

static bool is_tile_opaque_xy(int x, int y)
{
    auto [ s, t ] = st_of_xy(x, y);
    return is_tile_opaque(s, t);
}

static std::vector<std::tuple<Slope, Slope>> vis_ivls;
static std::vector<std::tuple<Slope, Slope>> next_vis_ivls;

static void process_one_rot()
{
    vis_ivls.clear();
    vis_ivls.push_back(make_tuple(Slope {0,1}, Slope {1,1}));

    for (int x = 2; !vis_ivls.empty(); ++x) {
        next_vis_ivls.clear();

        for (auto [ vis_open, vis_close ] : vis_ivls) {
            int a = vis_open.dy, b = vis_open.dx;
            int k = ((a+b)*x + 2*b) / (3*b);

            Slope next_vis_open = vis_open;

            while (true) {
                int yc = 3*k - x;

                Slope tile_open {yc-1, x}, tile_close {yc+1, x};

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

void compute_visibility(int origin_s, int origin_t)
{
    ::origin_s = origin_s;
    ::origin_t = origin_t;

    mark_tile_visible(origin_s, origin_t);
    for (nrot = 0; nrot < 6; ++nrot) {
        process_one_rot();
    }
}
