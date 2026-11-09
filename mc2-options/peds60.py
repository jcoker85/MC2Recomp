"""Midnight Club II (SLUS-20209): pedestrians' animations at the right speed with the 60 FPS patch.
    python -I peds60.py        writes peds60.pnach (section [Peds 60 FPS])

Found 2026-11-08 with the toolkit's memory watch on a recorded drive: the pedestrians' walk along their paths follows the game's time
step (0x432154, halved by the 60 FPS patch), but their animation player (0x29D2B0) adds a whole frame per call: frame (+10, int16) +=
step (+8, normally 1), wrapped by the clip's length. The ped manager (0x2993A0) calls it once per pedestrian per game update
(0x29A194, from 0x29A178), so at 60 updates a second the walk cycle played twice as fast as the body moved.
The patch sends that call through a few instructions in the free memory below the game (0x000FF400, beside the HUD patch's code at
0x000FF000..0x000FF154): with 60 fps on (the option word 000FFF02 = 1) every other game update (the game's update counter 0x432170,
odd) returns without advancing the animation, so it runs at the console's 30 frames a second; otherwise it jumps to the player as
before. The player itself is unchanged.
"""
import os

CAVE = 0x000FF400
CALL = 0x0029A194      # jal 0x0029D2B0 in the ped update (0x29A178)
PLAYER = 0x0029D2B0


def j(op, target):
    return (op << 26) | ((target >> 2) & 0x3FFFFFF)


def branch(op, rs, rt, at, target):
    return (op << 26) | (rs << 21) | (rt << 16) | (((target - at - 4) >> 2) & 0xFFFF)


V0, V1, ZERO = 2, 3, 0
code = [
    0x3C020010,                                  # lui  v0, 0x0010
    0x9442FF02,                                  # lhu  v0, -0xFE(v0)       the 60 fps option word 000FFF02
    None,                                        # beq  v0, zero, go        60 fps off: as before
    0x3C030043,                                  # lui  v1, 0x0043          (delay slot)
    0x8C632170,                                  # lw   v1, 0x2170(v1)      the game's update counter 0x432170
    0x30630001,                                  # andi v1, v1, 1
    None,                                        # beq  v1, zero, go        even updates advance
    0x00000000,                                  # nop
    0x03E00008,                                  # jr   ra                  odd updates: no frame step
    0x00000000,                                  # nop
    j(2, PLAYER),                                # go: j 0x0029D2B0         the player, returning to the ped update
    0x00000000,                                  # nop
]
GO = CAVE + 10 * 4
code[2] = branch(4, V0, ZERO, CAVE + 2 * 4, GO)
code[6] = branch(4, V1, ZERO, CAVE + 6 * 4, GO)

lines = ['gametitle=Midnight Club II (U)(SLUS-20209) E5F2DF38', '', '[Peds 60 FPS]',
         'author=made for this recompilation (mc2-options/peds60.py)',
         'description=Pedestrian animations at their own speed with the 60 FPS patch; switched by the 16-bit word at 000FFF02.',
         'patch=0,EE,%08X,word,%08X' % (CALL, j(3, CAVE))]
lines += ['patch=0,EE,%08X,word,%08X' % (CAVE + 4 * i, w) for i, w in enumerate(code)]
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'peds60.pnach')
with open(out, 'w', newline='\r\n') as f:
    f.write('\n'.join(lines) + '\n')
print(out)
