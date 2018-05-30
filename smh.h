#ifndef SMH_H
#define SMH_H

#include "common_defs.h"
#include <string>
#include <vector>

//#define DEBUG

#ifdef DEBUG
#include <iomanip>
#include <iostream>
inline void dump256(m256 d, std::string msg) {
        for (u32 i = 0; i < 32; i++) {
                std::cout << std::setw(3) << (int)*(((u8 *)(&d)) + i);
        if (!((i+1)%8))
            std::cout << "|";
        else if (!((i+1)%4))
            std::cout << ":";
        else
            std::cout << " ";
        }
    std::cout << " " << msg << "\n";
}

// dump bits low to high
void dumpbits(u64 v, std::string msg) {
        for (u32 i = 0; i < 64; i++) {
        std::cout << (((v>>(u64)i) & 0x1ULL) ? "1" : "_");
    }
    std::cout << " " << msg << "\n";
}

void dumpbits32(u32 v, std::string msg) {
        for (u32 i = 0; i < 32; i++) {
        std::cout << (((v>>(u32)i) & 0x1ULL) ? "1" : "_");
    }
    std::cout << " " << msg << "\n";
}
#else
#define dump256(a,b) ;
#define dumpbits(a,b) ;
#define dumpbits32(a,b) ;
#endif

const size_t SIMD_READ_WIDTH = 16;

#define LOOSE
inline void set_byte_at_offset(m256 & in, u32 offset, u8 content) {
    union {
        m256 sse;
        u8 bytes[32];
    } u;
    u.sse = in;
    u.bytes[offset] = content;
    in = u.sse;
}

struct SIMD_SMH_PART {
    m256 shuf_mask;
    m256 cmp_mask;
    m256 and_mask; // not yet used
    m256 sub_mask; // not yet used

    u32 doit(m256 d) {
#ifdef DEBUG
        dump256(d, "input");
        m256 shuf = _mm256_shuffle_epi8(d, shuf_mask);
        dump256(shuf_mask, "shuf_mask");
        dump256(shuf, "shuf result");
        m256 cmpres = _mm256_cmpeq_epi8(shuf, cmp_mask);
        dump256(cmp_mask, "cmp_mask");
        dump256(cmpres, "cmp result");
        std::cout << "\n";
        u32 r = _mm256_movemask_epi8(cmpres);
        return r;
#else
        return _mm256_movemask_epi8(
                   _mm256_cmpeq_epi8(_mm256_shuffle_epi8(d, shuf_mask),
                                     cmp_mask));
#endif
    }   
};

struct GPR_SMH_PART {
    u64 hi;
    u64 low;

    u64 doit(u64 m) {
#ifdef LOOSE
#ifdef DEBUG
        dumpbits(m, "input to gpr-smh");
        dumpbits(hi, "hi");
        dumpbits(low, "low");
        u64 after_add = m + low;
        dumpbits(after_add, "after_add");
        u64 ret = after_add & hi;
        dumpbits(ret, "ret");
        std::cout << "\n";
        return ret;
#else
        return (m + low) & hi;
#endif

#else

#ifdef DEBUG
        dumpbits(m, "input to gpr-smh");
        dumpbits(hi, "hi");
        dumpbits(low, "low");
        u64 holes = m & ~hi;
        dumpbits(holes, "holes");
        u64 after_add = holes + low;
        dumpbits(after_add, "after_add");
        u64 ret = after_add & (m & hi);
        dumpbits(ret, "ret");
        std::cout << "\n";
        return ret;
#else
        return ((m & ~hi) + low) & (m & hi);
#endif

#endif
    }
};

typedef enum { PREFIX, SUFFIX } SMH_MODE;

inline bool build_smh(size_t n_simd_parts, SIMD_SMH_PART * s,
                      size_t n_gpr_parts,  GPR_SMH_PART * g, 
                      size_t num_ids, u32 * ids,
                      std::vector<std::string> & strings,
                      std::vector<u32> & orig_ids,
                      SMH_MODE mode, 
                      bool loose) {

    assert(mode == PREFIX);
    assert(num_ids == n_gpr_parts*64 + 1);
    u32 loc = 0; // index corresponding to bytes in SIMD_SMH_PART and bits in GPR_SMH_PART
                 // We chunk SIMD_SMH_PART by 32 (for AVX2) and GPR_SMH_PART by 64 for 64-bit GPR
                 // thus all the loc/64 loc%64 loc/32 loc%32 nonsense.

    for (u32 i = 0; i < num_ids; i++) {
        ids[i] = 0;
    }
    for (u32 i = 0; i < n_simd_parts; i++) {
        // for now this is a nice way to get zero compare-results in our unused slots
        // this is cosmetic only for debugging but it's easier to deal with and it's pretty
        // unlikely that valgrind could figure out that such uninitialized stuff is never
        // affecting the computation
        s[i].shuf_mask = _mm256_set1_epi8(0x80);
        s[i].cmp_mask = _mm256_set1_epi8(0xff);
    }
    for (u32 i = 0; i < n_gpr_parts; i++) {
        g[i].low = 0;
        g[i].hi = 0;
    }

    // assign strings to locations. Walk strings in reverse order as 
    // we want earlier strings to take precedence (useful later for overlapping
    // string optimization)
    assert(strings.size() == orig_ids.size());
    auto rii = orig_ids.rbegin();
    for (auto rsi = strings.rbegin(), rse = strings.rend(); rsi != rse; ++rsi) {
        const std::string str = *rsi;
        u32 id = *rii++;

        size_t l = str.size();
        if (l > 16) {
            return false;
        } 

        u32 end_loc = loc+l-1 + (loose ? 1 : 0);
        // if we would cross into a different GPR block we need to move loc into that new block
        // and waste some space. Later we can do clever packing but for now, just waste the space
        if (end_loc/64 != (loc/64)) {
            loc = ((loc+63)/64 * 64);
            end_loc = loc+l-1 + (loose ? 1 : 0); // and recalculate end_loc
        }

        // bail out if we don't have room for the string
        if ((end_loc/64 > n_gpr_parts) || (end_loc/32 > n_simd_parts)) {
            return false;
        }

        g[loc/64].low |= (1ULL << (loc%64));
        u32 i = 0;
        for (auto c : str) {
            u32 location_in_input = (mode == PREFIX) ? i : (16 - l + i);
            i++;
            set_byte_at_offset(s[loc/32].shuf_mask, loc%32, location_in_input); 
            set_byte_at_offset(s[loc/32].cmp_mask, loc%32, c);
            loc++;
        }
        g[end_loc/64].hi |= (1ULL << (end_loc%64));
        if (loose) {
            loc++;
        }
        // we count leading zeros across all of our values and we need to set aside an 
        // empty slot for when we get a zero result (no match). So num_ids already has a +1,
        // so we need to subtract 1 from that for the zero-result and 1 from that to handle the
        // fact that the position of end_loc itself isn't part of the trailing zeros
        ids[(num_ids-2) - end_loc] = id;
    }
    return true;
}

class SMH32 {
    SIMD_SMH_PART s[1];
    GPR_SMH_PART g[1];
    u32 ids[65]; // padded, for regularization
public:
    SMH32(std::vector<std::string> & strings,
          std::vector<u32> & orig_ids) {
        build_smh(1, s, 1, g, 65, ids, strings, orig_ids, PREFIX, true);
    }
    really_inline u32 match(const u8 * buf, UNUSED const size_t len) {
        // TODO: find better intrinsic - there's a memory form of this instruction
        m256 d = _mm256_broadcastsi128_si256(*(const m128 *)buf);
        u32 m = s[0].doit(d); 
        u32 m_tmp = g[0].doit(m);
        u32 cnt = _lzcnt_u64(m_tmp); // TODO: figure out why we get a useless op after this
        return ids[cnt];
    }

    std::string name() { return "SMH32"; }
};

class SMH64 {
    SIMD_SMH_PART s[2];
    GPR_SMH_PART g[1];
    u32 ids[65];
public:
    SMH64(std::vector<std::string> & strings,
          std::vector<u32> & orig_ids) {
        build_smh(2, s, 1, g, 65, ids, strings, orig_ids, PREFIX, true);
    }

    really_inline u32 match(const u8 * buf, UNUSED const size_t len) {
        // TODO: find better instrinsic - there's a memory form of this instruction
        m256 d = _mm256_broadcastsi128_si256(*(const m128 *)buf);
        u64 m0 = s[0].doit(d); 
        u64 m1 = s[1].doit(d); 
        u64 m = m0 | (m1 << 32);
        u64 m_tmp = g[0].doit(m);
        u32 cnt = _lzcnt_u64(m_tmp); 
        return ids[cnt];
    }

    std::string name() { return "SMH64"; }
};

class SMH128 {
    SIMD_SMH_PART s[4];
    GPR_SMH_PART g[2];
    u32 ids[129];
public:
    SMH128(std::vector<std::string> & strings,
           std::vector<u32> & orig_ids) {
        build_smh(4, s, 2, g, 129, ids, strings, orig_ids, PREFIX, true);
    }

    really_inline u32 match(const u8 * buf, UNUSED const size_t len) {
        // TODO: find better instrinsic - there's a memory form of this instruction
        m256 d = _mm256_broadcastsi128_si256(*(const m128 *)buf);
        u64 m0 = s[0].doit(d); 
        u64 m1 = s[1].doit(d); 
        u64 m2 = s[2].doit(d); 
        u64 m3 = s[3].doit(d); 
        u64 m_lo = m0 | (m1 << 32);
        u64 m_hi = m2 | (m3 << 32);
        u64 m_tmp_lo = g[0].doit(m_lo);
        u64 m_tmp_hi = g[1].doit(m_hi);
        u32 cnt_lo = _lzcnt_u64(m_tmp_lo); 
        u32 cnt_hi = _lzcnt_u64(m_tmp_hi);
        u32 cnt = m_tmp_hi != 0 ? cnt_hi : (cnt_lo + 64);
#ifdef DEBUG
        dumpbits(m_tmp_lo, "m_tmp_lo");
        dumpbits(m_tmp_hi, "m_tmp_hi");
        std::cout << "cnt_lo: " << cnt_lo << "\n";
        std::cout << "cnt_hi: " << cnt_hi << "\n";
        std::cout << "cnt: " << cnt << "\n";
        std::cout << "\n";
#endif
        return ids[cnt];
    }

    std::string name() { return "SMH128"; }
};

#endif
