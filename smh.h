#ifndef SMH_H
#define SMH_H

#include "common_defs.h"
#include <string>
#include <vector>

const size_t SIMD_READ_WIDTH = 16;

//#define LOOSE
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
        return _mm256_movemask_epi8(
                   _mm256_cmpeq_epi8(_mm256_shuffle_epi8(d, shuf_mask),
                                     cmp_mask));
    }   
};

struct GPR_SMH_PART {
    u64 hi;
    u64 low;

    u64 doit(u64 m) {
#ifdef LOOSE
        return (m + low) & hi;
#else
        return ((m & ~hi) + low) & (m & hi);
#endif
    }
};

typedef enum { PREFIX, SUFFIX } SMH_MODE;

inline bool build_smh(size_t n_simd_parts, SIMD_SMH_PART * s,
                      size_t n_gpr_parts,  GPR_SMH_PART * g, 
                      size_t num_ids, u32 * ids,
                      std::vector<std::string> & strings,
                      std::vector<u32> & orig_ids,
                      SMH_MODE mode) {
    u32 loc = 0; // index corresponding to bytes in SIMD_SMH_PART and bits in GPR_SMH_PART
                 // We chunk SIMD_SMH_PART by 32 (for AVX2) and GPR_SMH_PART by 64 for 64-bit GPR
                 // thus all the loc/64 loc%64 loc/32 loc%32 nonsense.

    for (u32 i = 0; i < num_ids; i++) {
        ids[i] = 0;
    }

    // assign strings to locations. Walk strings in reverse order as 
    // we want higher ids to take precedence (useful later for overlapping
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

        u32 end_loc = loc+l-1;
#ifdef LOOSE
        end_loc++;
#endif
        // if we would cross into a different GPR block we need to move loc into that new block
        // and waste some space. Later we can do clever packing but for now, just waste the space
        if (end_loc/64 != (loc/64)) {
            loc = (loc+63)/64;
        }

        // bail out if we don't have room for the string
        if ((end_loc/64 > n_gpr_parts) || (end_loc/32 > n_simd_parts)) {
            return false;
        }

        g[loc/64].low |= (1 << (loc%64));
        u32 i = 0;
        for (auto c : str) {
            u32 location_in_input = (mode == PREFIX) ? i : (16 - l + i);
            i++;
            set_byte_at_offset(s[loc/32].shuf_mask, loc%32, location_in_input); 
            set_byte_at_offset(s[loc/32].and_mask, loc%32, c);
            loc++;
        }
        g[end_loc/64].hi |= (1 << (end_loc%64));
#ifdef LOOSE
        loc++;
#endif
        // we count leading zeros, so end_loc % 64 needs to be subtracted from 63 to yield
        // the right index to stash the zeros
        ids[63-(end_loc%64)] = id;
    }
    return true;
}

class SMH32 {
    SIMD_SMH_PART s[1];
    GPR_SMH_PART g[1];
    u32 ids[33];
public:
    SMH32(std::vector<std::string> & strings,
          std::vector<u32> & orig_ids) {
        build_smh(1, s, 1, g, 33, ids, strings, orig_ids, PREFIX);
    }
    really_inline u32 match(const u8 * buf, UNUSED const size_t len) {
        // TODO: find better instrinsic - there's a memory-form of this instruction
        m256 d = _mm256_broadcastsi128_si256(*(const m128 *)buf);
        u32 m = s[0].doit(d); 
        u32 m_tmp = g[0].doit(m);
        u32 cnt = _lzcnt_u32(m_tmp); // TODO: figure out why we get a useless op after this
        return ids[cnt];
    }
};

class SMH64 {
    SIMD_SMH_PART s[2];
    GPR_SMH_PART g[1];
    u32 ids[65];
public:
    SMH64(std::vector<std::string> & strings,
          std::vector<u32> & orig_ids) {
        build_smh(2, s, 1, g, 65, ids, strings, orig_ids, PREFIX);
    }

    really_inline u32 match(const u8 * buf, UNUSED const size_t len) {
        // TODO: find better instrinsic - there's a memory-form of this instruction
        m256 d = _mm256_broadcastsi128_si256(*(const m128 *)buf);
        u64 m0 = s[0].doit(d); 
        u64 m1 = s[1].doit(d); 
        u64 m = m0 | (m1 << 32);
        u32 m_tmp = g[0].doit(m);
        u32 cnt = _lzcnt_u32(m_tmp); 
        return ids[cnt];
    }
};

class SMH128 {
    SIMD_SMH_PART s[4];
    GPR_SMH_PART g[2];
    u32 ids[129];
public:
    SMH128(std::vector<std::string> & strings,
           std::vector<u32> & orig_ids) {
        build_smh(4, s, 2, g, 129, ids, strings, orig_ids, PREFIX);
    }

    really_inline u32 match(const u8 * buf, UNUSED const size_t len) {
        // TODO: find better instrinsic - there's a memory-form of this instruction
        m256 d = _mm256_broadcastsi128_si256(*(const m128 *)buf);
        u64 m0 = s[0].doit(d); 
        u64 m1 = s[1].doit(d); 
        u64 m2 = s[2].doit(d); 
        u64 m3 = s[3].doit(d); 
        u64 m_lo = m0 | (m1 << 32);
        u64 m_hi = m2 | (m3 << 32);
        u32 m_tmp_lo = g[0].doit(m_lo);
        u32 m_tmp_hi = g[1].doit(m_hi);
        u32 cnt_lo = _lzcnt_u32(m_tmp_lo); 
        u32 cnt_hi = _lzcnt_u32(m_tmp_hi);
        u32 cnt = m_tmp_lo == 0 ? (cnt_hi + 64) : cnt_lo;
        return ids[cnt];
    }
};

#endif
