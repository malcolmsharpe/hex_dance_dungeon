bool is_tile_opaque(int s, int t);
void mark_tile_visible(int s, int t);

void compute_visibility(int origin_s, int origin_t);
void compute_visibility_flood(int origin_s, int origin_t);

// Utilities

// Hex cube coordinates:
// - down-right: +S
// - up: +T
// - down-left: +P
int const NDIRS = 6;
int const DIR_DS[NDIRS] = {  1,  0, -1, -1,  0,  1 };
int const DIR_DT[NDIRS] = {  0,  1,  1,  0, -1, -1 };

// Convenience macros
#define FR(i,a,b) for(int i=(a);i<(b);++i)
#define FOR(i,n) FR(i,0,n)
#define BEND(v) (v).begin(),(v).end()
