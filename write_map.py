import json

map_str = '''
            # # # #
           # . . . #
          # . . . . #
   # # # # . . . . . # # # #
  # . . . + . . . . + . . . #
 # . . . . # . . . # . . . . #
# . . @ . . # # # # . . . . . #
 # . . . . # . . . # . . . . #
  # . . . # . . . . # . . . #
   # + # # . . . . . # # + #
  # . . . + . . . . # . . . #
 # . . . . # . . . # . . . . #
# . . . . . # # # # . . . . . #
 # . . . . # . . . # . . . . #
  # . . . + . . . . + . . . #
   # # # # . . . . . # # # #
          # . . . . #
           # . . . #
            # # # #
'''

map_json = {}
tiles = []

for row, line in enumerate(map_str.split('\n')):
    for col, ch in enumerate(line):
        s = (col + row)//2
        t = -row

        tile_type = ''
        if ch == '#':
            tile_type = 'wall'
        elif ch == '+':
            tile_type = 'floor' # TODO: actually door
        elif ch == '.':
            tile_type = 'floor'
        elif ch == '@':
            tile_type = 'floor'
            map_json['player_s'] = s
            map_json['player_t'] = t

        if tile_type:
            tiles.append({'s': s, 't': t, 'type': tile_type})

map_json['tiles'] = tiles

assert 'player_s' in map_json

json.dump(map_json, open('data/map.json', 'w'), indent=4)
