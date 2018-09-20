# Test of truncated strings
option fail 0
option malloc 0
new
ih aardvark_bear_dolphin_gerbil_jaguar 5
it meerkat_panda_squirrel_vulture_wolf 5
rh aardvark_bear_dolphin_gerbil_jaguar
reverse
rh meerkat_panda_squirrel_vulture_wolf
option length 30
rh meerkat_panda_squirrel_vulture
reverse
option length 28
rh aardvark_bear_dolphin_gerbil
option length 21
rh aardvark_bear_dolphin
reverse
option length 22
rh meerkat_panda_squirrel
option length 7
rh meerkat
reverse
option length 8
rh aardvark
option length 100
rh aardvark_bear_dolphin_gerbil_jaguar
reverse
rh meerkat_panda_squirrel_vulture_wolf
free
quit