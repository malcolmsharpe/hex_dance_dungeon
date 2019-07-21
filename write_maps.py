import json

def emit(path, map_str):
    map_json = {}
    tiles = []
    entities = []

    for row, line in enumerate(map_str.split('\n')):
        for col, ch in enumerate(line):
            s = (col + row)//2
            t = -row

            tile_rec = {}
            tile_type = ''
            if ch == ' ':
                pass
            elif ch == '#':
                tile_type = 'wall'
            elif ch == '-':
                tile_type = 'door'
                tile_rec['rotation'] = 0
            elif ch == '/':
                tile_type = 'door'
                tile_rec['rotation'] = 1
            elif ch == '\\':
                tile_type = 'door'
                tile_rec['rotation'] = 2
            elif ch == '.':
                tile_type = 'floor'
            elif ch == '@':
                tile_type = 'floor'
                map_json['player_s'] = s
                map_json['player_t'] = t
            elif ch == 'b':
                tile_type = 'floor'
                entities.append({'s': s, 't': t, 'type': 'enemy_bat_blue'})
            elif ch == 'B':
                tile_type = 'floor'
                entities.append({'s': s, 't': t, 'type': 'enemy_bat_red'})
            elif ch == 's':
                tile_type = 'floor'
                entities.append({'s': s, 't': t, 'type': 'enemy_slime_blue'})
            elif ch == 'g':
                tile_type = 'floor'
                entities.append({'s': s, 't': t, 'type': 'enemy_ghost'})
            elif ch == 'k':
                tile_type = 'floor'
                entities.append({'s': s, 't': t, 'type': 'enemy_skeleton_white'})
            else:
                print('Unrecognized tile', ch)
                assert 0

            if tile_type:
                tile_rec.update({'s': s, 't': t, 'type': tile_type})
                tiles.append(tile_rec)

    map_json['tiles'] = tiles
    map_json['entities'] = entities

    assert 'player_s' in map_json

    json.dump(map_json, open(path, 'w'), indent=4)

emit('data/map_bat.json', r'''
            # # # #
           # . . . #
          # . . . . #
   # # # # . . B . . # # # #
  # . . . \ . . . . / . . . #
 # . . . . # . . . # . B . . #
# . . @ . . # # # # . . . . . #
 # . . . . # . . . # . . B . #
  # . . . # . . . . # . . . #
   # - # # . . b . . # # - #
  # . . . \ . . . . # . . . #
 # . . . . # . . . # . . B . #
# . . b . . # # # # . b . . . #
 # . . . . # . . . # . . B . #
  # . . . / . b . . \ . . . #
   # # # # . . . . . # # # #
          # . . b . #
           # . . . #
            # # # #
''')

emit('data/map_slime.json', r'''
       # # # # # # # # # # # # #
      # . . . . . . . . . . . . #
     # . . . . . s . . . . . . . #
    # . . . s . . . . . . s . . . #
   # . . . . . . . . s . . . . . . #
  # . . @ . . . s . . . . . s . . . #
   # . . . . . . . . s . . . . . . #
    # . . . s . . . . . . s . . . #
     # . . . . . s . . . . . . . #
      # . . . . . . . . . . . . #
       # # # # # # # # # # # # #
''')

emit('data/map_skeleton.json', r'''
       # # # # # # # # # # # # #
      # . . . . . . . . . . . . #
     # . @ . . . . . . k . . . #
      # . . . # . . . . . # . . #
       # . . # # . k . . # # . . #
      # . . . . . . . . . . . . #
     # . . . . . . # # . . k . #
      # . k . . . # # . . . . . #
       # . . # . . . . . # # . . #
      # . . # # . k . . . # . . #
     # . k . . . . . . k . . . #
      # . . . . . . . . . . . . #
       # # # # # # # # # # # # #
''')

emit('data/map_skeleton_line.json', r'''
        # # # # # # # # # # # #
       # . . . . . . . . . . . #
      # . . . k . . . . k . . . #
     # . . . . k . . . k . . . . #
      # . . . . k . . k . . . . #
     # . . . . . . . . . . . . . #
      # . . . . . . . . . . . . #
     # . k k k . . @ . . k k k . #
      # . . . . . . . . . . . . #
     # . . . . . . . . . . . . . #
      # . . . . k . . k . . . . #
     # . . . . k . . . k . . . . #
      # . . . k . . . . k . . . #
       # . . . . . . . . . . . #
        # # # # # # # # # # # #
''')

emit('data/map_proto1.json', r'''
                # # # # #
               # . . . . #
              # . . k . . #
             # . . . . . . #
    # # # # # . . . . b . . # # # # #
   # . . . . # . . s . . . # . . . . #
  # . . . . . \ . . . . . / . . . . . #
 # . @ . . . . # . . . . # . . . . . . #
# . . . . . . . # # - # # . . . s . k . #
 # . . . . . . # . . . . # . . b . . . #
  # . . . . . # . . . . . # . . . . . #
   # . . . . # . . b . . . # . . . . #
    # # - # # . . . s . . . # # - # #
   # . . . . # . . . k . . # . . . . #
  # . . . . . # . . . . . / . . . . . #
 # . . s . . . # . . . . # . . . b . . #
# . . . . b . . # # - # # . . s . . . . #
 # . k . . . . # . . . . # . . . . . . #
  # . . . . . / . . . . . # . . . k . #
   # . . . . # . . . . . . # . . . . #
    # # # # # . . . . . k . # # # # #
             # . . b . . . #
              # . . . . . #
               # . . . . #
                # # # # #
''')

emit('data/map_proto2.json', r'''
          # # # # # #
         # . . . . . #
  # # # # . . . s . . # # # # # # #
 # . . . \ . b . . k . # . . . . k #
# . @ . . # . . . . . / . . . . . . #
 # . . . . # # - # # # . b . s . . . #
  # . . . # . . . . . # . . . . . k #
   # # - # # . b . . . # . . . . . #
  # . . . . # . . s . . # # # - # # #
 # . . . . . # . . . . k # . . . . . #
# . . s . . . # . . k . . \ . b . . . #
 # . . . b . . # . . . . . # . . s . . #
  # . k . . . # # # - # # # # . . . . . #
   # . . . . / . . . . . . . # . . . k #
    # # # # # . b . s . . k . # . k . #
             # . . . . . . . # # # # #
              # . . . . k . #
               # # # # # # #
''')

emit('data/map_mix.json', r'''
       # # # # # # # # # # # # # #
      # . # . . . . . # . . . . . #
     # . . # . g . . . # . . b . . #
    # . . . \ . . . . . # . . . . . #
   # . . . . # . . s . . \ . k . k . #
  # . @ . . . # . . . . . # . . . . . #
   # . . . . # # # # - # # # # - # # #
    # . . . / . . . . . . . . . . . #
     # . . # . . s . . . s . . . . #
      # . # b . . . b . . . k . . #
       # # . . s . . . s . . g . #
        # . . . . . . . . . . . #
       # # # - # # # # # - # # #
      # . . . . . . . . . . . #
     # . b . . . . . . . k . #
    # . . s . . s . . s . . #
   # . k . . . . . . . b . #
  # . . . . . . . . g . . #
   # # # # # # # # # # # #
''')
