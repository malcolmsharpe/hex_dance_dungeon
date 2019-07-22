#include <set>
#include <utility>
#include <vector>

#include "hex_dance_dungeon.hpp"

using std::make_pair;
using std::make_tuple;

static std::set<std::tuple<int,int>> mark;
static std::vector<std::tuple<int,int>> q;

static void enqueue(int s, int t)
{
    mark_tile_visible(s, t);
    auto entry = make_tuple(s,t);
    if (mark.find(entry) != mark.end()) return;
    if (is_tile_opaque(s, t)) return;
    mark.insert(entry);
    q.push_back(entry);
}

void compute_visibility_flood(int origin_s, int origin_t)
{
    mark.clear();
    q.clear();

    enqueue(origin_s, origin_t);

    while (!q.empty()) {
        auto [ s, t ] = q.back();
        q.pop_back();

        FOR(d,NDIRS) {
            int ds = DIR_DS[d];
            int dt = DIR_DT[d];

            enqueue(s+ds, t+dt);
        }
    }
}
