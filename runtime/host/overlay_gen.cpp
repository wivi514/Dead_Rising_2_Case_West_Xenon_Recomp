// See overlay_gen.h for why this exists. This file is a LINE-FOR-LINE port of the
// Python generator — tools/gen_kbm_icons.py (which itself borrows
// tools/gen_pc_options.py's .big reader/writer and LZX encoder) — and its contract
// is BYTE IDENTITY with it: same inputs, same output files, same bytes. Where a
// choice looks arbitrary (an iteration order, a tie-break in the Huffman
// package-merge) it is the Python's choice, kept so the identity gate stays exact.
// Read the Python first for the WHY of every transform: it carries the part-60/
// part-92 ladder that established it (the guest decoder's crash on degenerate LZX
// streams, the used-extent rule, the fecmn.tex and str_en.bcs size pins). This
// file only re-states the load-bearing ones.
//
// The one thing the Python does that this file does not: DRAW. The 26 key-cap
// chips are PIL-rendered in the dev tree and ship as finished DXT5 texel blobs
// (tools/release/kbm_chips/*.dxt — our art, no Capcom byte); this composes them
// into the player's own banks.
//
// The LZX encoder, the .big machinery, the .bcs helpers and the fecmn.tex
// container model are Case Zero's runtime/host/overlay_gen.cpp verbatim (their
// release §0, gated byte-identical against the same Python lineage there). The
// layers on top are Case West's own: no game_patched (this title's Visuals panel
// is a code hook and its banks are read loose), a size-pinned str_en.bcs with ONE
// "PRESS START" spelling, and the camera-bar glyph retarget in ingame.big.
//
// Error discipline: the Python is assert-dense on purpose (every container model
// mismatch is a hard stop, never a skip). The port keeps that with an internal
// exception caught at the two public entry points — a gate failure names itself in
// `err` and nothing half-written is left behind (outputs are written only after
// every gate of their layer passed).
#include "overlay_gen.h"

#include "host_paths.h"

#include <xex_patcher.h> // XenonUtils: the SAME lzxDecompress that unpacks the XEX

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace
{
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

struct GenError
{
    std::string msg;
};

[[noreturn]] void Refuse(const std::string& msg)
{
    throw GenError{msg};
}

// ---------------------------------------------------------------------------
// Bytes and files
// ---------------------------------------------------------------------------

Bytes ReadFileBytes(const fs::path& p)
{
    FILE* f = std::fopen(p.string().c_str(), "rb");
    if (!f)
        Refuse("cannot read " + p.string());
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    Bytes out(size_t(sz < 0 ? 0 : sz));
    if (!out.empty() && std::fread(out.data(), 1, out.size(), f) != out.size())
    {
        std::fclose(f);
        Refuse("short read on " + p.string());
    }
    std::fclose(f);
    return out;
}

void WriteFileBytes(const fs::path& p, const Bytes& b)
{
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    FILE* f = std::fopen(p.string().c_str(), "wb");
    if (!f)
        Refuse("cannot write " + p.string());
    if (!b.empty() && std::fwrite(b.data(), 1, b.size(), f) != b.size())
    {
        std::fclose(f);
        Refuse("short write on " + p.string());
    }
    std::fclose(f);
}

uint32_t LE32(const uint8_t* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
           (uint32_t(p[3]) << 24);
}
void PutLE32(uint8_t* p, uint32_t v)
{
    p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24);
}
uint32_t BE32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
uint16_t BE16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
void AppendLE32(Bytes& b, uint32_t v)
{
    b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8));
    b.push_back(uint8_t(v >> 16)); b.push_back(uint8_t(v >> 24));
}
void AppendBE32(Bytes& b, uint32_t v)
{
    b.push_back(uint8_t(v >> 24)); b.push_back(uint8_t(v >> 16));
    b.push_back(uint8_t(v >> 8)); b.push_back(uint8_t(v));
}
void AppendBE16(Bytes& b, uint16_t v)
{
    b.push_back(uint8_t(v >> 8)); b.push_back(uint8_t(v));
}

// ---------------------------------------------------------------------------
// LZX: decode (XenonUtils' decoder, per-chunk XMemCompress framing)
// ---------------------------------------------------------------------------

// A compressed `.big` entry: [BE u32 uncompressed][BE u32 window] then chunks of
// {BE u32 frameLen; 0xFF; BE u16 rawLen; BE u16 cmpLen; LZX bits (+ slack)}. Each
// chunk is an independent stream — the framing tools/big_decompress calls
// "interpretation A".
Bytes LzxDecodeEntry(const Bytes& stored, const char* what)
{
    if (stored.size() < 16)
        Refuse(std::string(what) + ": stored entry too small to be compressed");
    const uint32_t uncompressed = BE32(stored.data());
    const uint32_t window = BE32(stored.data() + 4);
    Bytes out(uncompressed, 0);
    size_t produced = 0;
    for (size_t o = 8; o + 4 <= stored.size();)
    {
        const uint32_t len = BE32(stored.data() + o);
        if (!len || len > stored.size() - o - 4)
            break;
        const uint8_t* frame = stored.data() + o + 4;
        if (len < 5 || frame[0] != 0xFF)
            Refuse(std::string(what) + ": chunk without the 0xFF XMemCompress header");
        const uint32_t rawLen = BE16(frame + 1);
        const uint32_t cmpLen = BE16(frame + 3);
        if (cmpLen > len - 5 || produced + rawLen > out.size())
            Refuse(std::string(what) + ": chunk lengths inconsistent with the stream header");
        if (lzxDecompress(frame + 5, cmpLen, out.data() + produced, rawLen,
                          window, nullptr, 0) != 0)
            Refuse(std::string(what) + ": lzxDecompress refused a chunk");
        produced += rawLen;
        o += 4 + len;
    }
    if (produced != uncompressed)
        Refuse(std::string(what) + ": decoded " + std::to_string(produced) + " of " +
               std::to_string(uncompressed) + " bytes");
    return out;
}

// ---------------------------------------------------------------------------
// LZX: encode. gen_pc_options.py lzx_encode_stream, ported symbol for symbol.
// ---------------------------------------------------------------------------

// Position-code tables for the 32 KB window.
constexpr int kLzxExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
                              8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

const std::vector<int>& LzxBase()
{
    static const std::vector<int> base = [] {
        std::vector<int> b;
        int v = 0;
        for (int e : kLzxExtra)
        {
            b.push_back(v);
            v += 1 << e;
        }
        return b;
    }();
    return base;
}

// Length-limited canonical Huffman lengths by package-merge. Tie-breaks follow the
// Python exactly: candidates ordered by (frequency, symbol tuple) — two candidates
// can never compare equal because the symbol tuples are all distinct — and the
// merged pairs concatenate their tuples in sorted order.
struct HuffPkg
{
    uint64_t freq;
    std::vector<int> syms;
    bool operator<(const HuffPkg& o) const
    {
        if (freq != o.freq)
            return freq < o.freq;
        return syms < o.syms;
    }
};

std::vector<int> HuffmanLengths(const std::vector<uint64_t>& freqs, int maxlen)
{
    std::vector<int> used;
    for (size_t s = 0; s < freqs.size(); ++s)
        if (freqs[s])
            used.push_back(int(s));
    std::vector<int> out(freqs.size(), 0);
    if (used.empty())
        return out;
    if (used.size() == 1)
    {
        // A legal, decoder-accepted degenerate tree, only reached by the LENGTH
        // tree and only when a block has exactly one distinct match length.
        out[used[0]] = 1;
        return out;
    }
    std::vector<HuffPkg> level;
    for (int s : used)
        level.push_back({freqs[s], {s}});
    for (int pass = 0; pass < maxlen - 1; ++pass)
    {
        std::vector<HuffPkg> prev = level;
        std::sort(prev.begin(), prev.end());
        std::vector<HuffPkg> next;
        for (size_t i = 0; i + 1 < prev.size(); i += 2)
        {
            HuffPkg m{prev[i].freq + prev[i + 1].freq, prev[i].syms};
            m.syms.insert(m.syms.end(), prev[i + 1].syms.begin(), prev[i + 1].syms.end());
            next.push_back(std::move(m));
        }
        for (int s : used)
            next.push_back({freqs[s], {s}});
        std::sort(next.begin(), next.end());
        level = std::move(next);
    }
    const size_t take = 2 * used.size() - 2;
    // level is already sorted (the loop appended it sorted).
    for (size_t i = 0; i < take && i < level.size(); ++i)
        for (int s : level[i].syms)
            out[s] += 1;
    // Kraft equality, checked in integers: sum of 2^(maxlen - l) must be 2^maxlen.
    uint64_t kraft = 0;
    for (int l : out)
        if (l)
            kraft += 1ull << (maxlen - l);
    if (kraft != (1ull << maxlen))
        Refuse("package-merge broke Kraft");
    return out;
}

// Canonical code assignment, identical to libmspack's table builder: sort by
// (length, symbol), codes count upward with left-justification per length.
struct HuffCode
{
    uint32_t code;
    int len;
};

std::map<int, HuffCode> CanonicalCodes(const std::vector<int>& lengths)
{
    std::vector<std::pair<int, int>> syms; // (length, symbol)
    for (size_t s = 0; s < lengths.size(); ++s)
        if (lengths[s])
            syms.push_back({lengths[s], int(s)});
    std::sort(syms.begin(), syms.end());
    std::map<int, HuffCode> codes;
    uint32_t code = 0;
    int prevLen = 0;
    for (auto [l, s] : syms)
    {
        code <<= (l - prevLen);
        codes[s] = {code, l};
        code += 1;
        prevLen = l;
    }
    return codes;
}

// The bit accumulator: MSB-first bits, packed at the end into 16-bit LITTLE-endian
// words — bit-exact against libmspack's reader.
struct BitSink
{
    std::vector<uint8_t> bits;
    void Put(uint32_t val, int n)
    {
        for (int b = n - 1; b >= 0; --b)
            bits.push_back(uint8_t((val >> b) & 1));
    }
    Bytes Pack() const
    {
        std::vector<uint8_t> padded = bits;
        while (padded.size() % 16)
            padded.push_back(0);
        Bytes payload;
        for (size_t k = 0; k < padded.size(); k += 16)
        {
            uint32_t v = 0;
            for (size_t b = k; b < k + 16; ++b)
                v = (v << 1) | padded[b];
            payload.push_back(uint8_t(v & 0xFF));
            payload.push_back(uint8_t(v >> 8));
        }
        return payload;
    }
};

// A real, small LZX compressor: greedy LZ77 over the full 32 KB window with
// per-chunk canonical Huffman trees, emitting VERBATIM blocks — the same shape the
// shipped encoder uses for the frontend text entries. WHY IT HAS TO BE REAL is the
// part-60 ladder in the Python's docstring: the guest decoder crashes with heap
// corruption on every degenerate stream (stored entries, literal-only fixed
// trees), and the cure is streams statistically like the shipped ones.
Bytes LzxEncodeStream(const Bytes& data)
{
    if (data.size() > 0x8000)
        Refuse("lzx_encode_stream: " + std::to_string(data.size()) +
               " bytes needs multiple chunks, and multi-chunk streams from this "
               "encoder are REFUSED by the guest decoder");
    Bytes out;
    AppendBE32(out, uint32_t(data.size()));
    AppendBE32(out, 0x8000);

    const Bytes& chunk = data; // single chunk by the assert above
    const int n = int(chunk.size());

    // ---- pass 1: greedy LZ77 with an R0/R1/R2-aware cost preference
    struct Op
    {
        bool lit;
        int a; // literal byte, or offset
        int b; // match length
    };
    std::vector<Op> ops;
    std::unordered_map<uint32_t, std::vector<int>> last; // 3-gram -> positions
    auto key3 = [&](int p) {
        return (uint32_t(chunk[p]) << 16) | (uint32_t(chunk[p + 1]) << 8) | chunk[p + 2];
    };
    int i = 0;
    while (i < n)
    {
        int bestLen = 0, bestOff = 0;
        if (i + 3 <= n)
        {
            auto it = last.find(key3(i));
            if (it != last.end())
            {
                const std::vector<int>& cand = it->second;
                const size_t lo = cand.size() > 64 ? cand.size() - 64 : 0;
                for (size_t c = cand.size(); c-- > lo;) // most recent first
                {
                    const int j = cand[c];
                    const int off = i - j;
                    if (off > 0x8000 - 2)
                        break;
                    int length = 3;
                    const int limit = std::min(257, n - i);
                    while (length < limit && chunk[j + length] == chunk[i + length])
                        ++length;
                    if (!bestLen || length > bestLen)
                    {
                        bestLen = length;
                        bestOff = off;
                        if (length >= 128)
                            break;
                    }
                }
            }
        }
        if (bestLen >= 3)
        {
            ops.push_back({false, bestOff, bestLen});
            for (int p = i; p < std::min(i + bestLen, n - 2); ++p)
                last[key3(p)].push_back(p);
            i += bestLen;
        }
        else
        {
            ops.push_back({true, chunk[i], 0});
            if (i + 3 <= n)
                last[key3(i)].push_back(i);
            i += 1;
        }
    }

    // ---- pass 2: resolve the R0-R2 offset history now, so the emitted symbols
    // are exactly the counted ones
    struct ROp
    {
        bool lit;
        int a; // literal byte, or formatted offset
        int b; // match length
    };
    std::vector<ROp> rops;
    {
        int R[3] = {1, 1, 1};
        for (const Op& op : ops)
        {
            if (op.lit)
            {
                rops.push_back({true, op.a, 0});
                continue;
            }
            const int off = op.a;
            int fmt;
            if (off == R[0])
                fmt = 0;
            else if (off == R[1])
            {
                fmt = 1;
                R[1] = R[0];
                R[0] = off;
            }
            else if (off == R[2])
            {
                fmt = 2;
                R[2] = R[0];
                R[0] = off;
            }
            else
            {
                fmt = off + 2;
                R[2] = R[1];
                R[1] = R[0];
                R[0] = off;
            }
            rops.push_back({false, fmt, op.b});
        }
    }

    const std::vector<int>& base = LzxBase();
    auto slotOf = [&](int fmt) {
        int slot = 0;
        for (int s = 0; s < 30; ++s)
            if (base[s] <= fmt)
                slot = s;
        return slot;
    };

    std::vector<uint64_t> mainFreq(496, 0); // 256 literals + 30 slots * 8
    std::vector<uint64_t> lenFreq(249, 0);
    for (const ROp& op : rops)
    {
        if (op.lit)
        {
            mainFreq[op.a] += 1;
            continue;
        }
        const int slot = slotOf(op.a);
        const int header = std::min(op.b - 2, 7);
        mainFreq[256 + slot * 8 + header] += 1;
        if (header == 7)
            lenFreq[op.b - 2 - 7] += 1;
    }

    std::vector<int> mainLen = HuffmanLengths(mainFreq, 16);
    std::vector<int> lengthLen = HuffmanLengths(lenFreq, 16);
    if (std::none_of(lengthLen.begin(), lengthLen.end(), [](int l) { return l != 0; }))
    {
        // No long matches this chunk: the tree would be EMPTY, and no shipped
        // stream has an empty LENGTH tree, so write a harmless two-code one.
        lengthLen[0] = lengthLen[1] = 1;
    }
    std::map<int, HuffCode> mainCodes = CanonicalCodes(mainLen);
    std::map<int, HuffCode> lengthCodes = CanonicalCodes(lengthLen);

    BitSink sink;

    // ---- tree-length writer: the delta/17/18 run format, with a real pretree per
    // READ_LENGTHS call, exactly as the decoder consumes it. prev is all-zero for
    // every call this encoder makes (each chunk is an independent stream).
    auto writeLengths = [&](const std::vector<int>& lens) {
        struct P
        {
            int sym;
            bool hasExtra;
            int extraVal, extraBits;
        };
        std::vector<P> ops2;
        size_t x = 0;
        while (x < lens.size())
        {
            if (lens[x] == 0)
            {
                size_t run = 0;
                while (x + run < lens.size() && lens[x + run] == 0)
                    ++run;
                while (run >= 20)
                {
                    const size_t take = std::min<size_t>(run, 51);
                    ops2.push_back({18, true, int(take - 20), 5});
                    run -= take;
                    x += take;
                }
                while (run >= 4)
                {
                    const size_t take = std::min<size_t>(run, 19);
                    ops2.push_back({17, true, int(take - 4), 4});
                    run -= take;
                    x += take;
                }
                for (size_t r = 0; r < run; ++r)
                {
                    ops2.push_back({0, false, 0, 0}); // (prev 0 - new 0) mod 17
                    ++x;
                }
            }
            else
            {
                ops2.push_back({((0 - lens[x]) % 17 + 17) % 17, false, 0, 0});
                ++x;
            }
        }
        std::vector<uint64_t> pf(20, 0);
        for (const P& p : ops2)
            pf[p.sym] += 1;
        std::vector<int> plens = HuffmanLengths(pf, 15);
        int usedCount = 0;
        for (int l : plens)
            if (l)
                ++usedCount;
        if (usedCount == 1)
        {
            // A one-code pretree: give it a legal complete partner.
            int lone = 0;
            for (int s = 0; s < 20; ++s)
                if (plens[s] == 1)
                {
                    lone = s;
                    break;
                }
            plens[lone] = 1;
            plens[(lone + 1) % 20] = 1;
        }
        std::map<int, HuffCode> pcodes = CanonicalCodes(plens);
        for (int s = 0; s < 20; ++s)
            sink.Put(uint32_t(plens[s]), 4);
        for (const P& p : ops2)
        {
            const HuffCode& c = pcodes.at(p.sym);
            sink.Put(c.code, c.len);
            if (p.hasExtra)
                sink.Put(uint32_t(p.extraVal), p.extraBits);
        }
    };

    sink.Put(0, 1); // no intel E8 translation
    sink.Put(1, 3); // LZX_BLOCKTYPE_VERBATIM
    sink.Put(uint32_t(n), 24);
    writeLengths(std::vector<int>(mainLen.begin(), mainLen.begin() + 256));
    writeLengths(std::vector<int>(mainLen.begin() + 256, mainLen.end()));
    writeLengths(lengthLen);

    // ---- content
    for (const ROp& op : rops)
    {
        if (op.lit)
        {
            const HuffCode& c = mainCodes.at(op.a);
            sink.Put(c.code, c.len);
            continue;
        }
        const int fmt = op.a;
        const int slot = slotOf(fmt);
        const int header = std::min(op.b - 2, 7);
        const HuffCode& c = mainCodes.at(256 + slot * 8 + header);
        sink.Put(c.code, c.len);
        if (header == 7)
        {
            const HuffCode& lc = lengthCodes.at(op.b - 2 - 7);
            sink.Put(lc.code, lc.len);
        }
        const int eb = kLzxExtra[slot];
        if (eb)
            sink.Put(uint32_t(fmt - base[slot]), eb);
    }

    const Bytes payload = sink.Pack();
    if (payload.size() >= size_t(n))
        Refuse("lzx_encode_stream did not compress (" + std::to_string(payload.size()) +
               " >= " + std::to_string(n) + ") — this encoder is for layout TEXT; "
               "do not point it at compressed data");
    // FIVE ZERO TRAILER BYTES inside the chunk length, after the payload — every
    // shipped chunk has exactly this, and it is decoder READAHEAD SLACK: the
    // guest's bit reader pulls 16-bit words past cmpLen at stream end.
    Bytes frame;
    frame.push_back(0xFF);
    AppendBE16(frame, uint16_t(n));
    AppendBE16(frame, uint16_t(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.insert(frame.end(), 5, 0);
    AppendBE32(out, uint32_t(frame.size()));
    out.insert(out.end(), frame.begin(), frame.end());
    return out;
}

// The round-trip gate the Python runs through tools/big_decompress: every emitted
// stream must decode, through the decoder we did NOT write, to the exact plaintext.
void VerifyEncodedStream(const Bytes& stored, const Bytes& want, const char* what)
{
    const Bytes got = LzxDecodeEntry(stored, what);
    if (got != want)
        Refuse(std::string(what) + ": encoded stream decoded to DIFFERENT bytes");
}

// ---------------------------------------------------------------------------
// .big archives (gen_pc_options.py read_big / write_big)
// ---------------------------------------------------------------------------

struct BigEntry
{
    std::string name;
    uint32_t nameOff, hash, size, size2, dataOff, flags, resv;
    Bytes stored;
};

struct BigArchive
{
    Bytes raw;
    uint32_t dataStart, namesOff;
    std::vector<BigEntry> entries;
};

// Split from Case Zero's ReadBig so the camera patch can verify a rebuilt archive
// without touching the filesystem: parse works on bytes, ReadBig is the file shell.
BigArchive ParseBig(Bytes raw, const std::string& label)
{
    BigArchive a;
    a.raw = std::move(raw);
    if (a.raw.size() < 24 || LE32(a.raw.data()) != 0x03040506)
        Refuse(label + ": bad .big magic");
    a.dataStart = LE32(a.raw.data() + 4);
    const uint32_t count = LE32(a.raw.data() + 12);
    const uint32_t hdr = LE32(a.raw.data() + 16);
    a.namesOff = LE32(a.raw.data() + 20);
    if (hdr != 0x18 || a.namesOff != 0x18 + count * 28)
        Refuse(label + ": unexpected .big header layout");
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t* e = a.raw.data() + 0x18 + i * 28;
        BigEntry be;
        be.nameOff = LE32(e);
        be.hash = LE32(e + 4);
        be.size = LE32(e + 8);
        be.size2 = LE32(e + 12);
        be.dataOff = LE32(e + 16);
        be.flags = LE32(e + 20);
        be.resv = LE32(e + 24);
        size_t end = be.nameOff;
        while (end < a.raw.size() && a.raw[end])
            ++end;
        be.name.assign(reinterpret_cast<const char*>(a.raw.data() + be.nameOff),
                       end - be.nameOff);
        be.stored.assign(a.raw.begin() + be.dataOff, a.raw.begin() + be.dataOff + be.size);
        a.entries.push_back(std::move(be));
    }
    return a;
}

BigArchive ReadBig(const fs::path& path)
{
    return ParseBig(ReadFileBytes(path), path.string());
}

// Rebuild: header + index + the ORIGINAL name table bytes + payloads packed in
// original file order. `align` preserves the source archive's own placement
// granularity (the frontend .big archives pack at 4 bytes). Mutates the entries'
// dataOff/size the way the Python's write_big mutates its dicts.
Bytes BuildBig(BigArchive& a, uint32_t align)
{
    const Bytes nameTable(a.raw.begin() + a.namesOff, a.raw.begin() + a.dataStart);
    std::vector<size_t> order(a.entries.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t x, size_t y) {
        return a.entries[x].dataOff < a.entries[y].dataOff;
    });
    uint32_t pos = a.dataStart;
    Bytes payload;
    for (size_t i : order)
    {
        BigEntry& e = a.entries[i];
        const uint32_t pad = (align - (pos % align)) % align;
        payload.insert(payload.end(), pad, 0);
        pos += pad;
        e.dataOff = pos;
        payload.insert(payload.end(), e.stored.begin(), e.stored.end());
        pos += uint32_t(e.stored.size());
        e.size = uint32_t(e.stored.size());
    }
    const uint32_t total = pos;
    Bytes out;
    AppendLE32(out, 0x03040506);
    AppendLE32(out, a.dataStart);
    AppendLE32(out, total);
    AppendLE32(out, uint32_t(a.entries.size()));
    AppendLE32(out, 0x18);
    AppendLE32(out, a.namesOff);
    for (const BigEntry& e : a.entries)
    {
        AppendLE32(out, e.nameOff);
        AppendLE32(out, e.hash);
        AppendLE32(out, e.size);
        AppendLE32(out, e.size2);
        AppendLE32(out, e.dataOff);
        AppendLE32(out, e.flags);
        AppendLE32(out, e.resv);
    }
    out.insert(out.end(), nameTable.begin(), nameTable.end());
    out.insert(out.end(), payload.begin(), payload.end());
    if (out.size() != total)
        Refuse("rebuilt .big archive size mismatch");
    return out;
}

// ---------------------------------------------------------------------------
// .bcs string banks ({u32 n; u32 ids[n]; u32 offs[n]; NUL-terminated strings})
// ---------------------------------------------------------------------------

std::map<uint32_t, Bytes> ParseBcs(const Bytes& d, const char* what,
                                   std::vector<uint32_t>* idOrder = nullptr)
{
    if (d.size() < 4)
        Refuse(std::string(what) + ": truncated .bcs");
    const uint32_t n = LE32(d.data());
    if (d.size() < 4 + 8ull * n)
        Refuse(std::string(what) + ": .bcs index truncated");
    std::map<uint32_t, Bytes> table;
    for (uint32_t k = 0; k < n; ++k)
    {
        const uint32_t id = LE32(d.data() + 4 + 4 * k);
        const uint32_t off = LE32(d.data() + 4 + 4 * n + 4 * k);
        size_t end = off;
        while (end < d.size() && d[end])
            ++end;
        table[id] = Bytes(d.begin() + off, d.begin() + end);
        if (idOrder)
            idOrder->push_back(id);
    }
    return table;
}

Bytes BuildBcs(const std::vector<uint32_t>& ids, const std::map<uint32_t, Bytes>& table)
{
    const uint32_t n = uint32_t(ids.size());
    const uint32_t header = 4 + 4 * n * 2;
    Bytes blob;
    std::vector<uint32_t> offs;
    for (uint32_t id : ids)
    {
        offs.push_back(header + uint32_t(blob.size()));
        const Bytes& s = table.at(id);
        blob.insert(blob.end(), s.begin(), s.end());
        blob.push_back(0);
    }
    Bytes out;
    AppendLE32(out, n);
    for (uint32_t id : ids)
        AppendLE32(out, id);
    for (uint32_t o : offs)
        AppendLE32(out, o);
    out.insert(out.end(), blob.begin(), blob.end());
    return out;
}

// ---------------------------------------------------------------------------
// fecmn.tex — the glyph bank (gen_kbm_icons.py)
// ---------------------------------------------------------------------------

// The title's own H33 name hash over the lowercase entry name.
uint32_t H33(const std::string& s)
{
    uint32_t v = 0;
    for (char ch : s)
        v = (v * 33) ^ uint32_t(uint8_t(std::tolower(uint8_t(ch))));
    return v;
}

struct TexEntry
{
    std::string name;
    uint32_t rec[7]; // {nameOffAbs, hash, size, 0x4030, payloadOffAbs, 4, 2}
};

std::vector<TexEntry> ParseTexBank(const Bytes& data)
{
    if (data.size() < 0x18)
        Refuse("fecmn.tex: truncated bank");
    const uint32_t count = LE32(data.data() + 0xC);
    std::vector<TexEntry> entries;
    for (uint32_t i = 0; i < count; ++i)
    {
        TexEntry e;
        for (int k = 0; k < 7; ++k)
            e.rec[k] = LE32(data.data() + 0x18 + 28 * i + 4 * k);
        size_t end = e.rec[0];
        while (end < data.size() && data[end])
            ++end;
        e.name.assign(reinterpret_cast<const char*>(data.data() + e.rec[0]), end - e.rec[0]);
        entries.push_back(std::move(e));
    }
    return entries;
}

// Rebuild the bank: entry table + name blob byte-identical, payloads re-laid-out
// in the original order with patched ones substituted.
Bytes RebuildTexBank(const Bytes& data, const std::vector<TexEntry>& entries,
                     const std::map<std::string, Bytes>& patches)
{
    std::vector<size_t> order(entries.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t x, size_t y) {
        return entries[x].rec[4] < entries[y].rec[4];
    });
    const uint32_t firstPay = entries[order[0]].rec[4];
    Bytes out(data.begin(), data.begin() + firstPay);
    for (size_t i : order)
    {
        const TexEntry& e = entries[i];
        Bytes pay;
        auto it = patches.find(e.name);
        if (it != patches.end())
            pay = it->second;
        else
            pay.assign(data.begin() + e.rec[4], data.begin() + e.rec[4] + e.rec[2]);
        while (out.size() & 3) // payloads are 4-byte aligned in the bank
            out.push_back(0);
        const uint32_t newOff = uint32_t(out.size());
        out.insert(out.end(), pay.begin(), pay.end());
        uint8_t* rec = out.data() + 0x18 + 28 * i;
        PutLE32(rec + 0, e.rec[0]);
        PutLE32(rec + 4, e.rec[1]);
        PutLE32(rec + 8, uint32_t(pay.size()));
        PutLE32(rec + 12, e.rec[3]);
        PutLE32(rec + 16, newOff);
        PutLE32(rec + 20, e.rec[5]);
        PutLE32(rec + 24, e.rec[6]);
    }
    PutLE32(out.data() + 0x8, uint32_t(out.size()));
    return out;
}

// ---------------------------------------------------------------------------
// The layer
// ---------------------------------------------------------------------------

struct Paths
{
    fs::path game;  // assets/game
    fs::path kbm;   // assets/game_kbm
    fs::path chips; // the shipped key-cap texel blobs, or empty if absent
};

fs::path FindChipsDir()
{
    std::error_code ec;
    const fs::path shipped = HostPaths::ExeDir() / "kbm_chips";
    if (fs::is_directory(shipped, ec))
        return shipped;
    const fs::path dev = HostPaths::Root() / "tools" / "release" / "kbm_chips";
    if (fs::is_directory(dev, ec))
        return dev;
    return {};
}

Paths MakePaths()
{
    Paths p;
    p.game = HostPaths::Game();
    p.kbm = p.game.parent_path() / (p.game.filename().string() + "_kbm");
    p.chips = FindChipsDir();
    return p;
}

// --- the camera-bar glyph retarget (gen_kbm_icons.py patch_camera_layout) ---
//
// The epilogue-camera HUD bar hard-codes its three prompts' glyphs as the MENU
// face-button icons, which our overlay legends X / Enter / Esc — wrong for the
// keyboard camera (LMB + wheel/keys 1,3). The atlas has no mouse or scroll icon
// and those menu glyphs are shared with every real menu bar, so the fix
// retargets the camera bar (only) at the _ig variants whose EXISTING legends
// are already truthful. Both Chuck and Frank bars live in cameraview.txt.
void PatchCameraLayout(const Paths& p)
{
    BigArchive a = ReadBig(p.game / "data" / "frontend" / "ingame.big");

    // GATE: identity repack of the untouched archive must be byte-exact.
    {
        BigArchive ident = a; // BuildBig mutates offsets; work on a copy
        if (BuildBig(ident, 4) != a.raw)
            Refuse("ingame.big GATE FAILED: identity repack is not byte-identical — "
                   "refusing to patch the camera layout");
    }

    BigEntry* cam = nullptr;
    for (BigEntry& e : a.entries)
        if (e.name == "cameraview.txt")
            cam = &e;
    if (!cam)
        Refuse("ingame.big: no cameraview.txt entry");
    Bytes dec = LzxDecodeEntry(cam->stored, "cameraview.txt");
    if (dec.size() != cam->size2)
        Refuse("ingame.big GATE FAILED: cameraview.txt decompressed to " +
               std::to_string(dec.size()) + " != " + std::to_string(cam->size2));

    const std::pair<const char*, const char*> subs[] = {
        {"File=\"x_button\"", "File=\"x_button_ig\""},
        {"File=\"a_button\"", "File=\"LBbutton_ig\""},
        {"File=\"b_button\"", "File=\"RBbutton_ig\""},
    };
    for (const auto& [oldS, newS] : subs)
    {
        const size_t oldLen = std::strlen(oldS);
        const size_t newLen = std::strlen(newS);
        // Count first (the Python counts before replacing): exactly the two bars.
        size_t count = 0;
        for (size_t s = 0; s + oldLen <= dec.size(); ++s)
            if (std::memcmp(dec.data() + s, oldS, oldLen) == 0)
                ++count;
        if (count != 2)
            Refuse("ingame.big GATE FAILED: cameraview.txt holds " +
                   std::to_string(count) + " of " + oldS +
                   ", expected 2 (Chuck + Frank bars) — layout moved");
        Bytes out2;
        out2.reserve(dec.size() + 2 * (newLen - oldLen));
        for (size_t s = 0; s < dec.size();)
        {
            if (s + oldLen <= dec.size() && std::memcmp(dec.data() + s, oldS, oldLen) == 0)
            {
                out2.insert(out2.end(), newS, newS + newLen);
                s += oldLen;
            }
            else
            {
                out2.push_back(dec[s]);
                ++s;
            }
        }
        dec = std::move(out2);
    }

    const Bytes pay = LzxEncodeStream(dec);
    VerifyEncodedStream(pay, dec, "cameraview.txt"); // round-trips through the real decoder
    cam->stored = pay;
    cam->size2 = uint32_t(dec.size());

    const Bytes out = BuildBig(a, 4);

    // Verify nothing else moved: parse the rebuilt archive; every non-camera
    // entry's stored bytes must be identical to the source archive's.
    const BigArchive check = ParseBig(out, "ingame.big (rebuilt)");
    if (check.entries.size() != a.entries.size())
        Refuse("ingame.big repack changed the entry count");
    size_t changed = 0;
    for (size_t k = 0; k < check.entries.size(); ++k)
        if (check.entries[k].name != "cameraview.txt" &&
            check.entries[k].stored != a.entries[k].stored)
            ++changed;
    if (changed)
        Refuse("ingame.big GATE FAILED: " + std::to_string(changed) +
               " non-camera entries changed");

    WriteFileBytes(p.kbm / "data" / "frontend" / "ingame.big", out);
}

// --- assets/game_kbm (gen_kbm_icons.py main) --------------------------------

void GenerateKbmLayer(const Paths& p,
                      const std::function<void(const char*, float)>& progress)
{
    const fs::path srcTex = p.game / "data" / "frontend" / "fecmn.tex";
    const Bytes data = ReadFileBytes(srcTex);
    const std::vector<TexEntry> entries = ParseTexBank(data);

    // GATE 1: identity repack — the container model must reproduce the shipped
    // bank byte for byte before anything is patched into it.
    if (RebuildTexBank(data, entries, {}) != data)
        Refuse("fecmn.tex GATE 1 FAILED: zero-patch rebuild is not byte-identical — "
               "the container model is wrong, refusing to write anything");

    std::map<std::string, Bytes> patches;
    // The DEVICE-FOLLOW sidecar: for every patched glyph, the 16-byte decoded-
    // record header (a unique in-memory fingerprint) plus BOTH texel sets, so the
    // runtime can swap the art in place when the active input device changes.
    struct SwapEntry
    {
        std::string base;
        Bytes fingerprint, padTex, kbTex;
    };
    std::vector<SwapEntry> swapEntries;
    size_t done = 0, patchable = 0;
    for (const TexEntry& e : entries)
    {
        const std::string base = e.name.substr(0, e.name.rfind('.'));
        std::error_code ec;
        if (fs::exists(p.chips / (base + ".dxt"), ec))
            ++patchable;
    }
    for (const TexEntry& e : entries)
    {
        const std::string base = e.name.substr(0, e.name.rfind('.'));
        const fs::path chipPath = p.chips / (base + ".dxt");
        std::error_code ec;
        if (!fs::exists(chipPath, ec))
            continue;
        if (progress)
        {
            char l[64];
            std::snprintf(l, sizeof l, "PREPARING KEY PROMPTS - %zu OF %zu", done + 1,
                          patchable);
            progress(l, 0.05f + 0.70f * float(done) / float(patchable ? patchable : 1));
        }
        // GATE 2: the stored hash must be the H33 name hash — content swaps leave
        // it untouched, so a mismatch means this is not the bank we know.
        if (e.rec[1] != H33(e.name))
            Refuse("fecmn.tex GATE 2 FAILED: " + e.name + " hash mismatch");
        const Bytes stored(data.begin() + e.rec[4], data.begin() + e.rec[4] + e.rec[2]);
        const uint32_t window = BE32(stored.data() + 4);
        if (window != 0x8000)
            Refuse("fecmn.tex: " + e.name + " window is not 0x8000");
        const Bytes old = LzxDecodeEntry(stored, e.name.c_str());
        if (old.size() < 48 || old[0] != 0x05 || old[1] != 0x01 || old[2] != 0x01 ||
            old[3] != 0xE6)
            Refuse("fecmn.tex: " + e.name + " is not the 05 01 01 E6 texture record "
                   "this generator understands");
        const Bytes hdr(old.begin(), old.begin() + 48);
        const Bytes texels(old.begin() + 48, old.end());
        const Bytes chip = ReadFileBytes(chipPath);
        if (chip.size() != texels.size())
            Refuse("fecmn.tex: " + e.name + " shipped chip is " +
                   std::to_string(chip.size()) + " bytes, the bank's texels are " +
                   std::to_string(texels.size()) + " — a different SKU's bank?");
        Bytes raw = hdr;
        raw.insert(raw.end(), chip.begin(), chip.end());
        const Bytes pay = LzxEncodeStream(raw);
        // GATE 3: round-trip through the real decompressor.
        VerifyEncodedStream(pay, raw, e.name.c_str());
        patches[e.name] = pay;
        swapEntries.push_back({base, Bytes(hdr.begin(), hdr.begin() + 16), texels, chip});
        ++done;
    }
    if (patches.empty())
        Refuse("fecmn.tex: no chip blob matched any bank entry — kbm_chips missing "
               "or wrong");

    if (progress)
        progress("PREPARING KEY PROMPTS - STRINGS", 0.78f);

    // THE STRING BANKS — Case West's variant, and every difference from Case Zero
    // is a measured fact recorded in the Python reference: (1) this title reads the
    // banks LOOSE from the disc tree, so the source is the shipped bank, not a
    // patched layer; (2) every bank is SIZE-PINNED (the loader reads a fixed byte
    // count and a shorter file blanks ALL UI text), so after the id-4049 table
    // rebuild each bank is padded back to exactly its shipped size; (3) en ships ONE
    // "PRESS START" spelling and NO PRESS\0START id pair (the sibling had both).
    //
    // ALL EIGHT banks are visited: the id-4049 LS->MASH rewrite is MECHANICS, not
    // prose, so it belongs in every language a player can pick. The English-literal
    // edits stay en-only — translating PRESS START / LEFT STICK is a content
    // decision the operator owns. Two banks are skipped by their own evidence
    // rather than by a special case: `id` is an IDENTIFIER bank (id 4049 reads
    // "IDS_HUD_LS", a QA aid with no prose in it), and `lg` has zero tail slack, so
    // MASH's extra byte cannot fit under the pin.
    {
        const fs::path frontend = p.game / "data" / "frontend";
        std::vector<std::string> langs;
        {
            std::error_code ec;
            for (const auto& e : fs::directory_iterator(frontend, ec))
            {
                const std::string f = e.path().filename().string();
                if (f.size() > 8 && f.compare(0, 4, "str_") == 0 &&
                    f.rfind(".bcs") == f.size() - 4)
                    langs.push_back(f.substr(4, f.size() - 8));
            }
            std::sort(langs.begin(), langs.end());   // the Python's sorted() order
        }
        bool wroteEn = false;
        for (const std::string& lang : langs)
        {
            const Bytes shipped = ReadFileBytes(frontend / ("str_" + lang + ".bcs"));
            const size_t strPin = shipped.size();
            Bytes sbank = shipped;
            if (lang == "en")
            {
                struct Edit
                {
                    const char* oldB;
                    size_t oldLen;
                    const char* newB;
                };
                const Edit edits[] = {
                    {"PRESS START\0", 12, "PRESS ENTER\0"},
                    {"LEFT STICK ", 11, "A / D KEYS "},
                };
                for (const Edit& ed : edits)
                {
                    size_t count = 0, at = 0;
                    for (size_t x = 0; x + ed.oldLen <= sbank.size(); ++x)
                        if (std::memcmp(sbank.data() + x, ed.oldB, ed.oldLen) == 0)
                        {
                            ++count;
                            at = x;
                        }
                    if (count != 1)
                        Refuse("str_en.bcs holds " + std::to_string(count) + " of a "
                               "title string expected exactly once — refusing the KB "
                               "edit");
                    std::memcpy(sbank.data() + at, ed.newB, ed.oldLen);
                }
            }
            // IDS_HUD_LS (id 4049) labels ONLY the struggle prompt; MASH does not fit
            // the shipped 2-3 byte value in place, so the table is rebuilt in the
            // shipped id order and every id verified to read back.
            std::vector<uint32_t> idOrder;
            std::map<uint32_t, Bytes> table =
                ParseBcs(sbank, ("str_" + lang + ".bcs").c_str(), &idOrder);
            const Bytes lsSpace = {'L', 'S', ' '};
            const Bytes lsBare = {'L', 'S'};
            auto it = table.find(4049);
            if (it == table.end() || (it->second != lsSpace && it->second != lsBare))
                continue;   // the identifier bank: no stick label, nothing to rewrite
            table[4049] = Bytes{'M', 'A', 'S', 'H'};
            Bytes rebuilt = BuildBcs(idOrder, table);   // keep the shipped id order
            if (ParseBcs(rebuilt, "rebuilt") != table)
                Refuse("rebuilt str_" + lang + ".bcs does not read back");
            if (rebuilt.size() > strPin)
                continue;   // no tail slack (lg): the loader would reject it
            rebuilt.insert(rebuilt.end(), strPin - rebuilt.size(), 0);
            WriteFileBytes(p.kbm / "data" / "frontend" / ("str_" + lang + ".bcs"),
                           rebuilt);
            if (lang == "en")
                wroteEn = true;
        }
        if (!wroteEn)
            Refuse("str_en.bcs was not written — the English bank is the one this "
                   "overlay cannot do without");
    }

    // glyph_swap.bin: the device-follow sidecar, PAD texels from the player's own
    // bank + our KB texels. No Capcom byte ships — both sets are composed here.
    {
        Bytes swp;
        swp.push_back('K'); swp.push_back('B'); swp.push_back('S'); swp.push_back('W');
        AppendLE32(swp, uint32_t(swapEntries.size()));
        for (const SwapEntry& se : swapEntries)
        {
            if (se.padTex.size() != se.kbTex.size())
                Refuse("glyph_swap: texel set size mismatch on " + se.base);
            AppendLE32(swp, uint32_t(se.base.size()));
            AppendLE32(swp, uint32_t(se.padTex.size()));
            swp.insert(swp.end(), se.base.begin(), se.base.end());
            swp.insert(swp.end(), se.fingerprint.begin(), se.fingerprint.end());
            swp.insert(swp.end(), se.padTex.begin(), se.padTex.end());
            swp.insert(swp.end(), se.kbTex.begin(), se.kbTex.end());
        }
        WriteFileBytes(p.kbm / "glyph_swap.bin", swp);
    }

    if (progress)
        progress("PREPARING KEY PROMPTS - CAMERA BAR", 0.88f);

    PatchCameraLayout(p);

    // The bank itself. THE SIZE PIN: layout.bin fixes fecmn.tex at the shipped
    // size and the loader reads by that size, so the patched bank must fit UNDER
    // it and is padded to EXACTLY it — one layout record serves both overlay
    // states.
    Bytes out = RebuildTexBank(data, entries, patches);
    if (out.size() > data.size())
        Refuse("fecmn.tex GATE FAILED: patched bank " + std::to_string(out.size()) +
               " bytes exceeds the shipped " + std::to_string(data.size()) +
               " that layout.bin pins — refusing to write");
    out.insert(out.end(), data.size() - out.size(), 0);
    PutLE32(out.data() + 0x8, uint32_t(out.size()));
    WriteFileBytes(p.kbm / "data" / "frontend" / "fecmn.tex", out);
}

// ---------------------------------------------------------------------------
// Wanted / stamp
// ---------------------------------------------------------------------------

// Bump when any transform above changes byte-for-byte output — INCLUDING a chip
// art change (re-export tools/release/kbm_chips with gen_kbm_icons.py
// --export-chips in the same commit): a shipped update must not keep serving a
// player's stale banks (the gotcha-13 shape, on disk).
constexpr int kGeneratorVersion = 2; // 1: first Case West generator (26 glyphs,
                                     //    camera bar, pinned str_en.bcs)
                                     // 2: id-4049 MASH in every language bank

fs::path StampPath(const Paths& p)
{
    return p.kbm / ".cw_overlay_version";
}

bool OutputsCurrent(const Paths& p)
{
    std::error_code ec;
    // The stamp carries the generator version; missing or older means regenerate.
    if (fs::exists(StampPath(p), ec))
    {
        FILE* f = std::fopen(StampPath(p).string().c_str(), "rb");
        if (f)
        {
            char buf[16] = {};
            (void)!std::fread(buf, 1, sizeof buf - 1, f);
            std::fclose(f);
            if (std::atoi(buf) != kGeneratorVersion)
                return false;
        }
    }
    else
        return false;

    const fs::path wanted[] = {
        p.kbm / "data" / "frontend" / "fecmn.tex",
        p.kbm / "data" / "frontend" / "str_en.bcs",   // the other language banks
                                                     // ride the version stamp
        p.kbm / "data" / "frontend" / "ingame.big",
        p.kbm / "glyph_swap.bin",
    };
    for (const fs::path& w : wanted)
        if (!fs::exists(w, ec))
            return false;
    return true;
}

bool EnvSet(const char* name)
{
    const char* v = std::getenv(name);
    return v && *v && std::strcmp(v, "0") != 0;
}

} // namespace

namespace OverlayGen
{
bool WantedAtBoot()
{
    if (EnvSet("CW_NO_OVERLAY_GEN"))
        return false;
    // These two asked for the shipped data / the pad input path; do not spend a
    // first-run generating files that run will then ignore (vfs.cpp gates the
    // overlay's USE on the same switches).
    if (std::getenv("CW_NO_PATCHED_ASSETS") || std::getenv("CW_NO_NATIVE_KBM"))
        return false;
    const Paths p = MakePaths();
    std::error_code ec;
    if (!fs::exists(p.game / "data" / "frontend" / "fecmn.tex", ec))
        return false; // game not unpacked yet; the first-run gate will say so
    if (p.chips.empty())
        return false; // nothing to compose from (dev tree without the export)
    return !OutputsCurrent(p);
}

bool Generate(const std::function<void(const char*, float)>& progress, std::string& err)
{
    try
    {
        const Paths p = MakePaths();
        if (p.chips.empty())
            Refuse("kbm_chips not found beside the executable or in tools/release — "
                   "keyboard prompt icons cannot be generated");
        GenerateKbmLayer(p, progress);
        // The stamp is written LAST: a failed run leaves no stamp, so the next
        // boot tries again rather than trusting a half-written layer.
        char buf[16];
        std::snprintf(buf, sizeof buf, "%d", kGeneratorVersion);
        WriteFileBytes(StampPath(p), Bytes(buf, buf + std::strlen(buf)));
        return true;
    }
    catch (const GenError& e)
    {
        err = e.msg;
        return false;
    }
}
}
