#include <map>
#include <set>
#include <utility>

enum class TileType
{
    none,
    floor,
    wall,
    door
};

extern std::map<std::pair<int,int>, TileType> tiles;
extern std::set<std::tuple<int,int>> is_visible;
extern int player_s, player_t;

void compute_visibility();

// Convenience macros
#define FR(i,a,b) for(int i=(a);i<(b);++i)
#define FOR(i,n) FR(i,0,n)
#define BEND(v) (v).begin(),(v).end()
