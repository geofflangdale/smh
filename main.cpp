#include <iostream>
#include <string.h>
#include <stdlib.h>
#include "smh.h"
#include <chrono>

const size_t CHUNK_SIZE = 32;
const u32 NUM_CHUNKS = 512;

#ifdef DEBUG
const int REPEATS = 1;
#else
const int REPEATS = 500000;
#endif

using namespace std;


template <typename T>
void match_multiple_smh(T & smh, std::vector<u8 *> & buffers, std::vector<size_t> & lengths,
                       std::vector<u32> & results) {
        u32 i = 0;
#ifndef NO_UNROLL
        for (; i+7 < buffers.size(); i+=8) {
            results[i+0] = smh.match(buffers[i+0], lengths[i+0]);
            results[i+1] = smh.match(buffers[i+1], lengths[i+1]);
            results[i+2] = smh.match(buffers[i+2], lengths[i+2]);
            results[i+3] = smh.match(buffers[i+3], lengths[i+3]);
            results[i+4] = smh.match(buffers[i+4], lengths[i+4]);
            results[i+5] = smh.match(buffers[i+5], lengths[i+5]);
            results[i+6] = smh.match(buffers[i+6], lengths[i+6]);
            results[i+7] = smh.match(buffers[i+7], lengths[i+7]);
        }
#endif
        for (; i < buffers.size(); ++i) {
            results[i] = smh.match(buffers[i], lengths[i]);
        }
}


//#define TRENTALIKE_DEMO

#ifdef TRENTALIKE_DEMO
const char * trent_strings[] = {
    "$AttrDef", "$BadClus", "$Bitmap", "$Boot", "$Extend", "$LogFile", "$MftMirr",
    "$Mft", "$Secure", "$UpCase", "$Volume", "$Cairo", "$INDEX_ALLOCATIO", "$DATA",
    "????", "."
};
#endif

template <typename T>
void demo(T & smh) {
    u8 * big_buf = new u8[CHUNK_SIZE];
#ifndef TRENTALIKE_DEMO
    const char * strs[] = { "mouse", "cat", "moose", "dog", "hippo", "this is long",
                            "also quite long", "getting quite d", "bored doing thi" };
#else
    const char * strs[] = {
        "$AttrDef", "$BadClus", "$Bitmap", "$Boot", "$Extend", "$LogFile", "$MftMirr",
        "$Mft", "$Secure", "$UpCase", "$Volume", "$Cairo", "$INDEX_ALLOCATIO", "$DATA",
        "????", "."
    };
#endif
    for (u32 i = 0; i < (sizeof(strs)/sizeof(const char *)); i++) {
        memset(big_buf, 0, 16);
        memcpy(big_buf, (void *)strs[i], strlen(strs[i]));
        u32 res = smh.match(big_buf, CHUNK_SIZE);
        cout << "Result: " << res << "\n\n";
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
    std::chrono::duration<double> secs_clock = end - start;
    double secs = secs_clock.count();
    u64 buffers_matched = NUM_CHUNKS * REPEATS;
    cout << "SMH variant: " << smh.name() << "\n"
         << "Matched " << buffers_matched << " buffers in " << secs << " seconds\n"
         << "Buffers/second: " << buffers_matched/secs  << "\n"
         << "Nanoseconds to match a single buffer (throughput): " << (secs*1000000000.0)/buffers_matched << "\n";
}


int main(UNUSED int argc, UNUSED char * argv[]) {
    vector<string> strings; 
    vector<u32> ids;
    // lame construction; too many interlocking cases to bother cleaning up
    strings.push_back(string("dog"));
    ids.push_back(10);
    strings.push_back(string("cat"));
    ids.push_back(20);
    strings.push_back(string("mouse"));
    ids.push_back(25);
    strings.push_back(string("moose"));
    ids.push_back(100);
    
#ifndef TRENTALIKE_DEMO
    SMH32 smh2(strings, ids); 
    demo(smh2);
    performance_test(smh2);

    SMH64 smh3(strings, ids); 
    demo(smh3);
    performance_test(smh3);

    strings.push_back(string("this is long"));
    ids.push_back(120);
    strings.push_back(string("also quite long"));
    ids.push_back(140); 
    strings.push_back(string("getting quite d"));
    ids.push_back(170); 
    strings.push_back(string("bored doing thi"));
    ids.push_back(180); 

    SMH128 smh(strings, ids); 
    demo(smh);
    performance_test(smh);
#else
    vector<string> tstr; 
    vector<u32> tids;
    for (u32 i = 0; i < (sizeof(trent_strings)/sizeof(const char *)); i++) {
        tstr.push_back(string(trent_strings[i]));
        tids.push_back(i);
    }
    SMH128 smh4(tstr, tids); 
    demo(smh4);
    performance_test(smh4);
#endif
}
