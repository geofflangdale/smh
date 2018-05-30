#include <iostream>
#include <string.h>
#include <stdlib.h>
#include "smh.h"
#include <chrono>

const size_t CHUNK_SIZE = 32;
const u32 NUM_CHUNKS = 512;
const int REPEATS = 500000;

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

void demo(UNUSED SMH128 & smh) {
}

never_inline void performance_test(SMH128 & smh) {
    u8 * big_buf = new u8[CHUNK_SIZE * NUM_CHUNKS];
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
    cout << "Matched " << buffers_matched << " buffers in " << secs << " seconds\n"
         << "Buffers/second: " << buffers_matched/secs  << "\n"
         << "Time to match a buffer: " << secs/buffers_matched << "\n";
}

int main(UNUSED int argc, UNUSED char * argv[]) {
    vector<string> strings; 
    vector<u32> ids;
    strings.push_back(string("dog"));
    ids.push_back(10);
    strings.push_back(string("cat"));
    ids.push_back(20);
    strings.push_back(string("mouse"));
    ids.push_back(25);
    strings.push_back(string("moose"));
    ids.push_back(100);
    
    SMH128 smh(strings, ids); 
    demo(smh);
    performance_test(smh);
}
