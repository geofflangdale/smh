#include <iostream>
#include <string.h>
#include <stdlib.h>
#include "smh.h"
#include <chrono>
#include <algorithm>

const size_t CHUNK_SIZE = 32;
const u32 NUM_CHUNKS = 512;

#ifdef DEBUG
const int REPEATS = 1;
#else
const int REPEATS = 500000;
#endif

using namespace std;

//#define GRATUITOUS_LFENCE_EVERYWHERE
//#define NO_UNROLL

#ifdef GRATUITOUS_LFENCE_EVERYWHERE
#define LFENCE _mm_lfence();
#else
#define LFENCE ;
#endif

template <typename T>
void match_multiple_smh(T & smh, std::vector<u8 *> & buffers, std::vector<size_t> & lengths,
                       std::vector<u32> & results) {
        u32 i = 0;
#ifndef NO_UNROLL
        for (; i+7 < buffers.size(); i+=8) {
            results[i+0] = smh.match(buffers[i+0], lengths[i+0]); LFENCE
            results[i+1] = smh.match(buffers[i+1], lengths[i+1]); LFENCE
            results[i+2] = smh.match(buffers[i+2], lengths[i+2]); LFENCE
            results[i+3] = smh.match(buffers[i+3], lengths[i+3]); LFENCE
            results[i+4] = smh.match(buffers[i+4], lengths[i+4]); LFENCE
            results[i+5] = smh.match(buffers[i+5], lengths[i+5]); LFENCE
            results[i+6] = smh.match(buffers[i+6], lengths[i+6]); LFENCE
            results[i+7] = smh.match(buffers[i+7], lengths[i+7]); LFENCE
        }
#endif
        for (; i < buffers.size(); ++i) {
            results[i] = smh.match(buffers[i], lengths[i]); LFENCE
        }
}

template <typename T>
void match_multiple_smh_latency_test(T & smh, std::vector<u8 *> & buffers, std::vector<size_t> & lengths,
                       std::vector<u32> & results) {
        u32 i = 0;
        u32 tmp = 0;
#ifndef NO_UNROLL
        // NOTE: experimental code only. Note that the addition of 'tmp' - being the id of a possible
        // match - could take us RIGHT outside our buffer if we actually matched something. We aren't
        // in this particular run, but so it goes. Saner would be to build up an all-zero id vector
        for (; i+7 < buffers.size(); i+=8) {
            tmp = results[i+0] = smh.match(buffers[i+0 + tmp], lengths[i+0] + tmp); LFENCE
            tmp = results[i+1] = smh.match(buffers[i+1 + tmp], lengths[i+1] + tmp); LFENCE
            tmp = results[i+2] = smh.match(buffers[i+2 + tmp], lengths[i+2] + tmp); LFENCE
            tmp = results[i+3] = smh.match(buffers[i+3 + tmp], lengths[i+3] + tmp); LFENCE
            tmp = results[i+4] = smh.match(buffers[i+4 + tmp], lengths[i+4] + tmp); LFENCE
            tmp = results[i+5] = smh.match(buffers[i+5 + tmp], lengths[i+5] + tmp); LFENCE
            tmp = results[i+6] = smh.match(buffers[i+6 + tmp], lengths[i+6] + tmp); LFENCE
            tmp = results[i+7] = smh.match(buffers[i+7 + tmp], lengths[i+7] + tmp); LFENCE
        }
#endif
        for (; i < buffers.size(); ++i) {
            tmp = results[i] = smh.match(buffers[i + tmp], lengths[i + tmp]); LFENCE
        }
}


template <typename T>
void demo(T & smh, vector<string> & demo_strs) {
    u8 * big_buf = new u8[CHUNK_SIZE];
    for (u32 i = 0; i < demo_strs.size(); i++) {
        memset(big_buf, 0, 16);
        memcpy(big_buf, (void *)demo_strs[i].c_str(), demo_strs[i].size());
        u32 res = smh.match(big_buf, CHUNK_SIZE);
        cout << "Result: " << res << "\n";
    }
}

template <typename T>
never_inline void performance_test(T & smh) {
    u8 * big_buf = new u8[CHUNK_SIZE * NUM_CHUNKS];
    for (u32 i = 0; i < CHUNK_SIZE * NUM_CHUNKS; i++) {
        big_buf[i] = (rand()%26) + 'a';
    }
    vector<u8 *> buffers;
    vector<size_t> lengths;
    vector<u32> results;
    buffers.resize(NUM_CHUNKS);
    lengths.resize(NUM_CHUNKS);
    results.resize(NUM_CHUNKS);
    for (u32 i = 0; i < NUM_CHUNKS; i++) {
        buffers[i] = &big_buf[i * CHUNK_SIZE];
        lengths[i] = CHUNK_SIZE;
    }

    auto start = std::chrono::steady_clock::now();
    for (u32 i = 0; i < REPEATS; i++) {
        match_multiple_smh(smh, buffers, lengths, results); 
    }
    auto end = std::chrono::steady_clock::now();

    auto start_lat = std::chrono::steady_clock::now();
    for (u32 i = 0; i < REPEATS; i++) {
        match_multiple_smh_latency_test(smh, buffers, lengths, results); 
    }
    auto end_lat = std::chrono::steady_clock::now();

    std::chrono::duration<double> secs_clock = end - start;
    double secs = secs_clock.count();

    std::chrono::duration<double> secs_clock_lat = end_lat - start_lat;
    double secs_lat = secs_clock_lat.count();

    u64 buffers_matched = NUM_CHUNKS * REPEATS;
    cout << "SMH variant: " << smh.name() << "\n"
         << "Matched " << buffers_matched << " buffers in " << secs << " seconds\n"
         << "Nanoseconds to match a single buffer (throughput): "
         << (secs*1000000000.0)/buffers_matched << "\n"
         << "Nanoseconds to match a single buffer (latency): " 
         << (secs_lat*1000000000.0)/buffers_matched << "\n";
}

//#define TRENTALIKE_DEMO

int main(UNUSED int argc, UNUSED char * argv[]) {
#ifndef TRENTALIKE_DEMO
    SMH_WORKLOAD work = { { "dog", 10}, { "cat", 20 }, { "mouse", 25 }, { "moose", 100 } };

    SMH_WORKLOAD work_long = { { "dog", 10}, { "cat", 20 }, { "mouse", 25 }, { "moose", 100 },
                               { "this is long", 120} , { "also quite long", 140 },
                               { "getting quite", 170} , { "bored doing this", 180 } };

    vector<string> demo_strings; // pull out the longer strings as our workload
    transform(work_long.begin(), work_long.end(), back_inserter(demo_strings),
              [](auto & p) -> string { return p.first; });

    SMH32<true> smh32(work);
    demo(smh32, demo_strings);
    performance_test(smh32);

    SMH32<false> smh32_tight(work);
    demo(smh32_tight, demo_strings);
    performance_test(smh32_tight);

    SMH64<true> smh64(work);
    demo(smh64, demo_strings);
    performance_test(smh64);

    SMH64<false> smh64_tight(work);
    demo(smh64_tight, demo_strings);
    performance_test(smh64_tight);

    SMH128<true> smh128(work_long);
    demo(smh128, demo_strings);
    performance_test(smh128);

    SMH128<false> smh128_tight(work_long);
    demo(smh128_tight, demo_strings);
    performance_test(smh128_tight);

#else
    const char * trent_strings[] = {
        "$AttrDef", "$BadClus", "$Bitmap", "$Boot", "$Extend", "$LogFile", "$MftMirr",
        "$Mft", "$Secure", "$UpCase", "$Volume", "$Cairo", "$INDEX_ALLOCATIO", "$DATA",
        "????", "."
    };
    SMH_WORKLOAD work;
    vector<string> test_strings;
    for (u32 i = 0; i < (sizeof(trent_strings)/sizeof(const char *)); i++) {
        work.push_back(make_pair(string(trent_strings[i]), i));
        test_strings.push_back(string(trent_strings[i]));
    }
    SMH128<true> smh128(work);
    demo(smh128, test_strings);
    performance_test(smh128);
#endif
}
