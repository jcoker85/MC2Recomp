"""Midnight Club II (SLUS-20209): keeps the in-race HUD in 4:3 shapes on a 16:9 picture, each part at its own edge.

    python -I hud43.py        writes hud43.pnach (a PCSX2-style patch file, section [HUD 4:3])

How it works (found with the toolkit's draw trace and an EE RAM dump):
  * 0x1C0C48..0x1C0E48: the race HUD's draw function calls the Draw method (vtable slot 11) of each of its parts in turn
    with `jalr ra, v0` (a0 = the part). Each call becomes `jal` to a small stub that loads that part's mark and goes on
    to WRAP. When the HUD option word at 0x000FFF04 is 1, WRAP adds one qword to the game's VIF1 packet chain (the same
    chain its 2D and 3D drawing goes into, pointer at 0x70003A40) before the part draws and one after:
    DMA tag cnt/0, VIF FLUSH, VIF MARK value. The toolkit's runtime (gif.cpp, hud_mark) squeezes the x of everything
    the part draws on PATH1/PATH2 while the mark is on: x -> A + (x - A) * 3/4 toward the part's edge A (left, centre,
    right), so the 16:9 picture, which widens the frame by 4/3, shows the part in its 4:3 shape. The parts are drawn
    through rectangles, text, VU1 triangle meshes and small 3D viewports (the minimap): the vertices and the scissor box
    are squeezed wherever they come from, which hooking the game's 2D builders could not do.
  * mark = 0x8000 | part << 8 | side (1 left, 2 centre, 3 right, 0 the third its first drawing is in;
    PS2_HUD_LOG=1 prints each part's box).
Memory used, all below the game's load address 0x100000 (nothing there is used by the game or the runtime):
  0x000FF000..  the new code;  0x000FFF04 HUD option (16 bit).
"""
import os

R = {n: i for i, n in enumerate(['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
                                 's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra'])}
CAVE = 0x000FF000
HUD_OPTION = -0xFC  # 0x000FFF04, from 0x00100000
PACKET_AT, PACKET_END = 0x3A40, 0x3A44  # the game's VIF1 packet pointer and its end, in the scratchpad (0x70000000)
PACKET_GROW = 0x3740A8  # the game's own call for room when the buffer is full: a0 = qwords, returns where to write them
NONE, LEFT, CENTRE, RIGHT = 0, 1, 2, 3

# The HUD's Draw calls, in the order the function makes them (the object field each part comes from), and the side each part
# belongs to, read off PS2_HUD_LOG=1 while driving (which third of the 512-pixel frame the part's box is in). Parts that drew
# nothing there (race-only ones) keep NONE: the runtime then takes the third their first drawing is in.
SITES = [  # (address of `jalr ra, v0`, field, side); part number = place in this list
    (0x1C0C54, 0x4C, NONE),                          # 0
    (0x1C0CCC, 0x20, RIGHT),   # 1   speedometer ring, x 377..500
    (0x1C0CE0, 0x10, CENTRE),  # 2   bar at the bottom, x 144..370
    (0x1C0CF4, 0x24, RIGHT),   # 3   gauge strip, x 449..487
    (0x1C0D08, 0x28, NONE), (0x1C0D1C, 0x2C, NONE), (0x1C0D30, 0x30, NONE), (0x1C0D44, 0x38, NONE),  # 4-7
    (0x1C0D58, 0x3C, NONE), (0x1C0D6C, 0x40, NONE),  # 8, 9
    (0x1C0D80, 0x18, RIGHT),   # 10  speedometer dial, x 378..506
    (0x1C0D94, 0x1C, RIGHT),   # 11  gear, x 476..489
    (0x1C0DA8, 0x14, RIGHT),   # 12  speed digits, x 414..466
    (0x1C0DBC, 0x44, NONE), (0x1C0DD0, 0x48, NONE),  # 13, 14
    (0x1C0DEC, 0x08, LEFT),    # 15  minimap: masks x 11..154, map triangles (clipped by the scissor) -155..447
    (0x1C0E1C, 0x0C, NONE), (0x1C0E48, 0x4C, NONE),  # 16, 17
]


class Asm:
    def __init__(self, base):
        self.base, self.items, self.labels = base, [], {}

    def label(self, name):
        self.labels[name] = self.base + 4 * len(self.items)

    def emit(self, f):
        self.items.append(f)

    def r(self, fn, rd='zero', rs='zero', rt='zero', sa=0):
        self.emit(lambda L, pc: (R[rs] << 21) | (R[rt] << 16) | (R[rd] << 11) | (sa << 6) | fn)

    def i(self, op, rt, rs, imm):
        self.emit(lambda L, pc: (op << 26) | (R[rs] << 21) | (R[rt] << 16) | (imm & 0xFFFF))

    def or_(self, d, s, t): self.r(37, d, s, t)
    def daddu(self, d, s, t): self.r(45, d, s, t)
    def sltu(self, d, s, t): self.r(43, d, s, t)
    def jr(self, s): self.r(8, 'zero', s)
    def jalr(self, s): self.r(9, 'ra', s)
    def nop(self): self.emit(lambda L, pc: 0)
    def addiu(self, t, s, imm): self.i(9, t, s, imm)
    def ori(self, t, s, imm): self.i(13, t, s, imm)
    def lui(self, t, imm): self.i(15, t, 'zero', imm)
    def lw(self, t, off, s): self.i(35, t, s, off)
    def lhu(self, t, off, s): self.i(37, t, s, off)
    def sw(self, t, off, s): self.i(43, t, s, off)

    def branch(self, op, s, t, target):
        self.emit(lambda L, pc: (op << 26) | (R[s] << 21) | (R[t] << 16) | (((L[target] - pc - 4) >> 2) & 0xFFFF))

    def beq(self, s, t, target): self.branch(4, s, t, target)
    def b(self, target): self.branch(4, 'zero', 'zero', target)
    def j(self, target): self.emit(lambda L, pc: (2 << 26) | (((L[target] if isinstance(target, str) else target) >> 2) & 0x3FFFFFF))
    def jal(self, target): self.emit(lambda L, pc: (3 << 26) | (((L[target] if isinstance(target, str) else target) >> 2) & 0x3FFFFFF))

    def words(self):
        return [(self.base + 4 * k, f(self.labels, self.base + 4 * k)) for k, f in enumerate(self.items)]


def build():
    a = Asm(CAVE)

    # WRAP (t8 = this part's mark, from its stub; v0 = its Draw method, a0 = the part)
    a.label('wrap')
    a.addiu('sp', 'sp', -32)
    a.sw('ra', 0, 'sp')
    a.sw('v0', 4, 'sp')
    a.sw('a0', 8, 'sp')
    a.sw('t8', 12, 'sp')
    a.lui('t9', 0x0010)
    a.lhu('t9', HUD_OPTION, 't9')
    a.beq('t9', 'zero', 'wrap_call')
    a.sw('t9', 16, 'sp')
    a.jal('mark')
    a.lw('a0', 12, 'sp')
    a.label('wrap_call')
    a.lw('v0', 4, 'sp')
    a.jalr('v0')
    a.lw('a0', 8, 'sp')
    a.lw('t9', 16, 'sp')
    a.beq('t9', 'zero', 'wrap_out')
    a.nop()
    a.jal('mark')
    a.daddu('a0', 'zero', 'zero')
    a.label('wrap_out')
    a.lw('ra', 0, 'sp')
    a.jr('ra')
    a.addiu('sp', 'sp', 32)

    # MARK (a0 = the value): one qword on the VIF1 chain, DMA tag cnt with no data, VIF FLUSH, VIF MARK a0
    a.label('mark')
    a.addiu('sp', 'sp', -16)
    a.sw('ra', 0, 'sp')
    a.sw('a0', 4, 'sp')
    a.lui('a3', 0x7000)
    a.lw('a2', PACKET_AT, 'a3')
    a.lw('v0', PACKET_END, 'a3')
    a.addiu('v1', 'a2', 16)
    a.sltu('v0', 'v0', 'v1')
    a.beq('v0', 'zero', 'mark_room')
    a.nop()
    a.jal(PACKET_GROW)
    a.addiu('a0', 'zero', 1)
    a.b('mark_write')
    a.daddu('a2', 'v0', 'zero')
    a.label('mark_room')
    a.sw('v1', PACKET_AT, 'a3')
    a.label('mark_write')
    a.lui('v0', 0x1000)
    a.sw('v0', 0, 'a2')       # DMA tag: cnt, qwc 0 (its upper half goes to the VIF)
    a.sw('zero', 4, 'a2')
    a.lui('v0', 0x1100)
    a.sw('v0', 8, 'a2')       # VIF FLUSH
    a.lw('a0', 4, 'sp')
    a.lui('v0', 0x0700)
    a.or_('v0', 'v0', 'a0')
    a.sw('v0', 12, 'a2')      # VIF MARK value
    a.lw('ra', 0, 'sp')
    a.jr('ra')
    a.addiu('sp', 'sp', 16)

    # one stub per Draw call: the part's mark, then WRAP
    hooks = []
    for k, (site, field, side) in enumerate(SITES):
        name = 'stub%d' % k
        a.label(name)
        a.j('wrap')
        a.ori('t8', 'zero', 0x8000 | (k << 8) | side)
        hooks.append((site, name))

    code = a.words()
    return code, [(site, (3 << 26) | ((a.labels[name] >> 2) & 0x3FFFFFF)) for site, name in hooks]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    code, hooks = build()
    lines = ['gametitle=Midnight Club II (U)(SLUS-20209) E5F2DF38', '', '[HUD 4:3]', 'author=made for this recompilation (mc2-options/hud43.py)',
             'description=Keeps the race HUD in 4:3 shapes at the screen edges with 16:9 on; switched by the 16-bit word at 000FFF04.']
    lines += ['patch=0,EE,%08X,word,%08X' % (addr, w) for addr, w in code]
    lines += ['patch=0,EE,%08X,word,%08X' % (addr, w) for addr, w in hooks]
    with open(os.path.join(here, 'hud43.pnach'), 'w', newline='\n') as out:
        out.write('\n'.join(lines) + '\n')
    print('%d words of new code at %08X..%08X, %d hooked Draw calls' % (len(code), code[0][0], code[-1][0] + 4, len(hooks)))


main()
