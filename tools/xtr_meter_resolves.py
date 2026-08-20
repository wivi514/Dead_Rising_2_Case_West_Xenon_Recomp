#!/usr/bin/env python3
"""For every draw in a .xtr trace, was RB_MODECONTROL edram_mode 6 (resolve) or not,
split by whether the bound VS is the meter shader vs_a4ae7c2b7c1818c4?

WHY: our PM4 walk sees >1M draws with that VS bound and EVERY one has mode 6, so our
renderer consumes them all as resolves. Hardware draws the widgets with that VS. The
adjudicating fact is hardware's OWN modecontrol at those draws: if hardware says 4
(normal draw), our register state is stale/wrong at those points; if hardware also
says 6, the widgets really are produced through resolves and DoResolve is the suspect.
"""
import sys, os, struct, collections
sys.path.insert(0, os.path.expanduser(
    '~/GithubRepo/Dead_Rising_2_Case_West_Xenon_Recomp/tools'))
import xtr
from xtr_draw_bindings import Memory, decompress, fnv1a, BANKS, DRAW_OPCODES, BE

METER = 'a4ae7c2b7c1818c4'

def main(path):
    data, hdr = xtr.open_trace(path)
    print('%s  %.1f MB' % (path, hdr['size'] / 1e6))
    mem = Memory(); regs = {}; bound = {0: None, 1: None}
    seq = 0; ndraw = 0; since = 0
    tally = collections.Counter()   # (is_meter, mode) -> count
    meter_rows = []
    for off, cmd in xtr.walk(data, len(data)):
        seq += 1
        if cmd in (xtr.CMD_MEMORY_READ, xtr.CMD_MEMORY_WRITE):
            base, enc, elen, dlen = struct.unpack_from('<IIII', data, off + 4)
            try:
                mem.add(base, decompress(data[off+20:off+20+elen], enc, dlen), seq)
            except Exception:
                pass
            continue
        if cmd != xtr.CMD_PACKET_START:
            continue
        count = struct.unpack_from('<I', data, off + 8)[0]
        if not count: continue
        header = BE.unpack_from(data, off + 12)[0]
        def word(i): return BE.unpack_from(data, off + 12 + 4*i)[0]
        ptype = header >> 30
        if ptype == 0:
            reg = header & 0x7FFF; one = (header >> 15) & 1
            for i in range(count - 1):
                regs[reg if one else reg + i] = word(1 + i)
            continue
        if ptype == 1:
            if count >= 3:
                regs[header & 0x7FF] = word(1)
                regs[(header >> 11) & 0x7FF] = word(2)
            continue
        if ptype != 3: continue
        opcode = (header >> 8) & 0x7F
        if opcode == 0x2D and count >= 2:
            base_reg = BANKS.get((word(1) >> 16) & 0xFF)
            if base_reg is not None:
                idx = word(1) & 0x7FF
                for i in range(2, count):
                    regs[base_reg + idx + i - 2] = word(i)
        elif opcode in (0x55, 0x56) and count >= 2:
            idx = word(1) & 0xFFFF
            for i in range(2, count):
                regs[idx + i - 2] = word(i)
        elif opcode == 0x27 and count >= 3:
            bound[word(1) & 3] = (word(1) & ~3, word(2) & 0xFFFF)
        elif opcode == 0x2B and count >= 3:
            size = word(2) & 0xFFFF
            code = b''.join(struct.pack('>I', word(3+i))
                            for i in range(min(size, count - 3)))
            bound[word(1) & 3] = ('inline', code)
        elif opcode in DRAW_OPCODES and count >= 2:
            init_at = 2 if opcode == 0x22 else 1
            if count <= init_at: continue
            init = word(init_at)
            vs = ''
            b = bound[0]
            if b is not None:
                if b[0] == 'inline':
                    vs = '%016x' % fnv1a(b[1])
                else:
                    code = mem.read(b[0], b[1]*4)
                    if code: vs = '%016x' % fnv1a(code)
            mode = regs.get(0x2208, 0)
            is_meter = (vs == METER)
            tally[(is_meter, mode & 7)] += 1
            if is_meter and (mode & 7) == 6:
                if len(meter_rows) < 90:
                    meter_rows.append((ndraw, since, regs.get(0x2319,0),
                                       regs.get(0x231A,0), regs.get(0x2318,0)))
                since = 0
            else:
                since += 1
            ndraw += 1
    print('total draws %d' % ndraw)
    for (is_meter, mode), n in sorted(tally.items()):
        print('  %s  modecontrol&7=%d : %d draws'
              % ('METER VS' if is_meter else 'other VS', mode, n))
    print('meter resolves (draw idx, NORMAL DRAWS SINCE PREV METER RESOLVE, dest, pitch, ctl):')
    for r in meter_rows[:90]:
        print('  draw %4d  since=%-4d dest=%08X pitch=%08X ctl=%08X' % r)

if __name__ == '__main__':
    main(sys.argv[1])
