bool is_tile_opaque(int s, int t);
void mark_tile_visible(int s, int t);

void compute_visibility(int origin_s, int origin_t);

// Convenience macros
#define FR(i,a,b) for(int i=(a);i<(b);++i)
#define FOR(i,n) FR(i,0,n)
#define BEND(v) (v).begin(),(v).end()
