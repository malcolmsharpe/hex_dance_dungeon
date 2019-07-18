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

bool operator<=(Slope const & a, Slope const & b)
{
    return !(b < a);
}

static int nrot;
static std::tuple<int,int> st_of_xy(int x, int y)
{
    int t = y/3;
    int s = (x-t)/2;
    int p = -s-t;

    FOR(i,nrot) {
        // 60-degree clockwise rotation
        std::tie(s,t,p) = make_tuple(-p,-s,-t);
    }
    
    return make_tuple(player_s+s, player_t+t);
}

struct TileRec
{
    int x,y1,y2;
    int s,t;
    TileType type;

    TileRec(int x_, int yc)
        : x(x_)
        , y1(yc - 2)
        , y2(yc + 2)
    {
        std::tie(s, t) = st_of_xy(x, yc);
        auto it = tiles.find(make_pair(s,t));
        if (it == tiles.end()) {
            type = TileType::none;
        } else {
            type = it->second;
        }
    }

    void mark_visible()
    {
        is_visible.insert(make_tuple(s,t));
    }
};

static std::vector<TileRec> tilerecs;

enum class MergeType
{
    open_shadow,
    open_tile,
    close_tile,
    close_shadow,
};

static std::vector<std::tuple<Slope, MergeType, TileRec*>> events;
static std::vector<std::tuple<Slope, Slope>> shadows;

static void process_one_rot()
{
    shadows.clear();

    for (int x = 2; ; ++x) {
        int yc;
        if (x%2 == 0) {
            yc = -6*((x+7)/6 - 1);
        } else {
            yc = -3 - 6*((x+4)/6 - 1);
        }

        tilerecs.clear();
        events.clear();

        for (auto [ slope1, slope2 ] : shadows) {
            events.push_back(make_tuple(slope1, MergeType::open_shadow, nullptr));
            events.push_back(make_tuple(slope2, MergeType::close_shadow, nullptr));
        }

        while (true) {
            TileRec rec(x, yc);
            if (x <= rec.y1) break;
            tilerecs.push_back(rec);

            yc += 6;
        }

        for (auto& rec : tilerecs) {
            Slope slope1 = Slope(rec.y1, rec.x);
            if (slope1 < Slope(-1,1)) slope1 = Slope(-1,1);
            Slope slope2 = Slope(rec.y2, rec.x);
            if (Slope(1,1) < slope2) slope2 = Slope(1,1);

            events.push_back(make_tuple(slope1, MergeType::open_tile, &rec));
            events.push_back(make_tuple(slope2, MergeType::close_tile, &rec));
        }

        sort(BEND(events));

        shadows.clear();

        TileRec * active_rec = nullptr;
        bool is_shadowed = false;
        int shadow_cast = 0;

        bool is_shadow_open = false;
        Slope shadow_start_slope;

        for (auto [ slope, mtype, recptr ] : events) {
            switch (mtype) {
            case MergeType::open_shadow: {
                is_shadowed = true;
                ++shadow_cast;
                break;
            }
            case MergeType::open_tile: {
                if (recptr->type != TileType::floor) ++shadow_cast;
                active_rec = recptr;
                break;
            }
            case MergeType::close_tile: {
                if (recptr->type != TileType::floor) --shadow_cast;
                if (recptr == active_rec) active_rec = nullptr;
                break;
            }
            case MergeType::close_shadow: {
                is_shadowed = false;
                --shadow_cast;
                break;
            }
            }

            if (shadow_cast > 0 && !is_shadow_open) {
                is_shadow_open = true;
                shadow_start_slope = slope;
            }

            // active tile is visible
            if (!is_shadowed && active_rec) {
                active_rec->mark_visible();
            }

            // store merged shadow
            if (shadow_cast == 0 && is_shadow_open) {
                is_shadow_open = false;
                shadows.push_back(make_tuple(shadow_start_slope, slope));
            }
        }

        // Abort when shadow covers entire arc
        if (shadows.size() == 1) {
            auto [ slope1, slope2 ] = shadows[0];

            if (slope1 <= Slope(-1,1) && Slope(1,1) <= slope2) break;
        }
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
