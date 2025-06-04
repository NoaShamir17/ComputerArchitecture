#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include <algorithm>
#include <iomanip>
#include <cstring>   // for strcmp
#include <cstdlib>   // for std::stoul

// Global constant used as a practical "infinite" timestamp for LRU comparisons
constexpr unsigned INFTY = (1u << 32) - 1;  // 0xFFFFFFFF

// ---------------------- Block & CacheLevel ----------------------

// Represents a single cache block (line)
class Block {
public:
    unsigned tag;         // Tag portion of stored address
    bool     valid;       // Valid bit: true if this block holds valid data
    bool     dirty;       // Dirty bit: true if block has been written to (needs write-back)
    unsigned lastAccess;  // Timestamp of last access (for LRU)

    // Constructor: initially invalid and clean, time = 0
    Block() : tag(0), valid(false), dirty(false), lastAccess(0) {}
};

// Simulates one level of cache (either L1 or L2)
class CacheLevel {
public:
    // existing members ...
public:
    unsigned size;        // Total cache size (bytes)
    unsigned block_size;  // Block size (bytes)
    unsigned assoc;       // Number of ways (fully associative if equals num lines per set)
    unsigned access_time; // Access latency (cycles)
    unsigned num_sets;    // Number of sets in cache (size / (block_size * assoc))

    // 2D array: sets[set_index][way_index] gives a Block
    std::vector<std::vector<Block>> sets;

    static unsigned global_clock; // Monotonically increasing timestamp for LRU

    // Constructor: compute number of sets and allocate vector of sets
    CacheLevel(unsigned size_, unsigned block_size_, unsigned assoc_, unsigned access_time_)
        : size(size_), block_size(block_size_), assoc(assoc_), access_time(access_time_) 
    {
        unsigned blocks = size / block_size;      // total blocks in this cache
        num_sets = blocks / assoc;                // sets = total blocks / ways
        // Initialize each set with 'assoc' Blocks, all invalid
        sets.resize(num_sets, std::vector<Block>(assoc));
    }

    // rd: Attempt to read from this cache level.
    //   address: full memory address
    //   evicted_tag/was_dirty: references to capture eviction info if miss occurs
    // Returns true on hit, false on miss (and calls load_block to bring block in).
    bool read(unsigned address, unsigned& evicted_tag, bool& was_dirty) {
        ++global_clock;                                  // increment global LRU clock
        unsigned idx = get_index(address);               // calculate set index
        unsigned tg  = get_tag(address);                 // extract tag
        
        // Search the set for matching tag
        for (auto& blk : sets[idx]) {
            if (blk.valid && blk.tag == tg) {
                // Cache hit: update LRU timestamp and return
                blk.lastAccess = global_clock;
                return true;
            }
        }
        // Cache miss: load block (possibly evict LRU) and capture eviction info
        load_block(address, evicted_tag, was_dirty);
        return false;
    }

    // wr: Attempt to write to this cache level.
    //   On hit, set dirty bit. On miss, load then mark dirty.
    bool write(unsigned address, unsigned& evicted_tag, bool& was_dirty) {
        ++global_clock;
        unsigned idx = get_index(address);
        unsigned tg  = get_tag(address);
        
        // Search the set for matching tag
        for (auto& blk : sets[idx]) {
            if (blk.valid && blk.tag == tg) {
                // Hit: update LRU timestamp, set dirty, return
                blk.lastAccess = global_clock;
                blk.dirty = true;
                return true;
            }
        }
        // Miss: bring block in (captures eviction), then mark it dirty
        if(write_allocate) {
            load_block(address, evicted_tag, was_dirty);
        }
        // Mark newly loaded block as dirty
        // unsigned idx2 = get_index(address);
        // unsigned tg2  = get_tag(address);
        // for (auto& blk : sets[idx2]) {
        //     if (blk.valid && blk.tag == tg2) {
        //         blk.dirty = true;
        //         return false;
        //     }
        // }
        return false;
    }

    // Evicts one block based on LRU or chooses an invalid slot if available.
    //   address: address to insert
    //   evicted_tag/was_dirty: set to info about evicted block (if valid)
    void load_block(unsigned address, unsigned& evicted_tag, bool& was_dirty) {
        unsigned idx = get_index(address);
        unsigned tg  = get_tag(address);
        auto& set = sets[idx];

        // Find either an invalid block or the LRU block
        unsigned lru_idx = 0;
        unsigned min_acc = INFTY;  // start "infinite" so any valid timestamp is smaller
        for (unsigned i = 0; i < assoc; ++i) {
            if (!set[i].valid) {
                // Empty slot found: choose it immediately
                lru_idx = i;
                min_acc = 0;
                break;
            } else if (set[i].lastAccess < min_acc) {
                // This block has a smaller LRU timestamp
                min_acc = set[i].lastAccess;
                lru_idx = i;
            }
        }
        // Capture eviction info if block is valid
        if (set[lru_idx].valid) {
            evicted_tag = set[lru_idx].tag;
            was_dirty   = set[lru_idx].dirty;
        } else {
            evicted_tag = 0;
            was_dirty   = false;
        }

        // Install new block: set fields and mark valid
        set[lru_idx].tag = tg;
        set[lru_idx].valid = true;
        set[lru_idx].dirty = false;
        set[lru_idx].lastAccess = global_clock;
    }

    // Invalidate any block matching this address (inclusive eviction).
    void invalidate(unsigned address) {
        unsigned idx = get_index(address);
        unsigned tg  = get_tag(address);
        for (auto& blk : sets[idx]) {
            if (blk.valid && blk.tag == tg) {
                blk.valid = false;
                blk.dirty = false;
                blk.lastAccess = 0;
            }
        }
    }

    // Give Cache access to internal fields for address computation
    friend class Cache;

    


private:
    // Compute set index from address
    unsigned get_index(unsigned address) const {
        return (address / block_size) % num_sets;
    }
    // Compute tag from address
    unsigned get_tag(unsigned address) const {
        return (address / block_size) / num_sets;
    }
};

// Initialize static clock
unsigned CacheLevel::global_clock = 0;

// ---------------------- Cache Coordinator ----------------------

// Manages two levels (L1 & L2), handling inclusive policy, write-back, write-allocate.
class Cache {
public:
    CacheLevel* L1;            // Pointer to L1 cache object
    CacheLevel* L2;            // Pointer to L2 cache object
    unsigned    mem_cycles;    // Latency for main memory access (cycles)
    bool        write_allocate; // If true, use write-allocate policy on write misses

    unsigned l1_misses = 0;    // Number of L1 misses
    unsigned l2_misses = 0;    // Number of L2 misses
    unsigned total_accesses = 0;     // Total number of memory accesses
    unsigned total_access_time = 0;  // Sum of access time for each request

    // Constructor: pass in two CacheLevel instances, memory latency, and write-allocate flag
    Cache(CacheLevel* l1, CacheLevel* l2, unsigned mem_cyc, bool wr_alloc)
        : L1(l1), L2(l2), mem_cycles(mem_cyc), write_allocate(wr_alloc)
    {}

    // Destructor: free both cache levels
    ~Cache() {
        delete L1;
        delete L2;
    }

    // Externally called for each memory access
    void access(bool is_write, unsigned address) {
        ++total_accesses;
        if (is_write)
            handle_write(address);
        else
            handle_read(address);
    }

private:
    // Handle a read from address
    void handle_read(unsigned address) {
        unsigned evicted_tag;
        bool was_dirty;

        // 1) Try L1
        if (L1->read(address, evicted_tag, was_dirty)) {
            total_access_time += L1->access_time;
            return;  // Hit in L1
        }
        // L1 miss: record stats and add L1 access time
        ++l1_misses;
        total_access_time += L1->access_time;

        // If L1 evicted a dirty block, update the block in L2 (mark it dirty and update LRU)
        if (was_dirty) {
            unsigned ev_addr = ((evicted_tag * L1->num_sets) + L1->get_index(address)) * L1->block_size;
            unsigned idx = L2->get_index(ev_addr);
            unsigned tg  = L2->get_tag(ev_addr);
            for (auto &blk : L2->sets[idx]) {
                if (blk.valid && blk.tag == tg) {
                    blk.dirty = true;
                    blk.lastAccess = CacheLevel::global_clock;
                }
            }
        }

        // 2) Try L2
        unsigned evicted_tag2;
        bool was_dirty2;
        if (L2->read(address, evicted_tag2, was_dirty2)) {
            total_access_time += L2->access_time;  // Hit in L2
            // Load the block into L1 (inclusive)
            unsigned tmp_tag2;
            bool tmp_dirty2;
            L1->load_block(address, tmp_tag2, tmp_dirty2); // is this because we need to update LRU?- bc we already loaded the block in L1->read
            return;
        }
        // L2 miss: record stats and add L2 + memory times
        ++l2_misses;
        total_access_time += L2->access_time + mem_cycles;

        // If L2 evicted a dirty block, assume write-back to memory happens
        if (was_dirty2) {
            // No additional latency counted
        }
        // Inclusive policy: if L2 evicted something, remove it from L1 as well
        if (L2->sets.empty() == false && evicted_tag2) {
            unsigned ev_addr2 = ((evicted_tag2 * L2->num_sets) + L2->get_index(address)) * L2->block_size;
            L1->invalidate(ev_addr2);
        }

        // 3) Fetch from memory, install in L2 then install in L1
        unsigned tmp_tag3;
        bool tmp_dirty3;
        L2->load_block(address, tmp_tag3, tmp_dirty3);
        L1->load_block(address, tmp_tag3, tmp_dirty3);
    }

    // Handle a write to address
    void handle_write(unsigned address) {
        // Branch depending on write-allocate policy
        if (!write_allocate) {
            // -------- No‑Write‑Allocate path --------
            ++total_accesses; // already counted in access()
            unsigned idxL1 = L1->get_index(address);
            unsigned tgL1  = L1->get_tag(address);

            // 1) Check L1 hit manually (without allocating)
            bool l1_hit = false;
            for (auto &blk : L1->sets[idxL1]) {
                if (blk.valid && blk.tag == tgL1) {
                    blk.dirty = true;
                    blk.lastAccess = ++CacheLevel::global_clock;
                    l1_hit = true;
                    break;
                }
            }
            if (l1_hit) {
                total_access_time += L1->access_time;
                return;
            }
            // L1 miss
            ++l1_misses;
            total_access_time += L1->access_time;

            // 2) Check L2 hit without allocating in L1
            if (L2->write_no_allocate(address)) {
                total_access_time += L2->access_time;
                return; // update done in L2 only
            }
            // 3) Miss in both levels -> write directly to memory
            ++l2_misses;
            total_access_time += L2->access_time + mem_cycles;
            return;
        }

        // -------- Write‑Allocate path --------
        unsigned evicted_tag;
        bool was_dirty;

        // 1) Try L1 write (will allocate on miss inside CacheLevel)
        if (L1->write(address, evicted_tag, was_dirty)) {
            total_access_time += L1->access_time;
            return;  // Hit in L1
        }
        // L1 miss
        ++l1_misses;
        total_access_time += L1->access_time;

        // If L1 evicted a dirty block, push it to L2
        if (was_dirty) {
            unsigned ev_addr = ((evicted_tag * L1->num_sets) + L1->get_index(address)) * L1->block_size;
            unsigned tmp_tag;
            bool tmp_dirty;
            L2->write(ev_addr, tmp_tag, tmp_dirty);
        }

        // 2) Try L2 write (allocates in L2)
        unsigned evicted_tag2;
        bool was_dirty2;
        if (L2->write(address, evicted_tag2, was_dirty2)) {
            total_access_time += L2->access_time;
            // Bring block into L1 and mark dirty
            unsigned tmp_tag2;
            bool tmp_dirty2;
            L1->load_block(address, tmp_tag2, tmp_dirty2);
            L1->write(address, tmp_tag2, tmp_dirty2);
            return;
        }
        // L2 miss
        ++l2_misses;
        total_access_time += L2->access_time + mem_cycles;

        // If L2 evicted a dirty block, assume write-back to memory
        if (was_dirty2) {
            // No additional latency
        }
        // Inclusive policy: invalidate in L1 if L2 evicted a block
        if (L2->sets.empty() == false && evicted_tag2) {
            unsigned ev_addr2 = ((evicted_tag2 * L2->num_sets) + L2->get_index(address)) * L2->block_size;
            L1->invalidate(ev_addr2);
        }

        // Fetch line from memory into L2 and then L1, then perform write
        unsigned tmp_tag3;
        bool tmp_dirty3;
        L2->load_block(address, tmp_tag3, tmp_dirty3);
        L1->load_block(address, tmp_tag3, tmp_dirty3);
        L1->write(address, tmp_tag3, tmp_dirty3);
    }
};

// ---------------------- main() + CLI + Trace Loop ----------------------

// Print usage and exit if arguments are incorrect
static void usage_and_exit(const char* progname) {
    std::cerr << "Usage: " << progname
              << " <trace_file> --mem-cyc <num> --bsize <log2(block)>"
                 " --wr-alloc <0|1> --l1-size <log2(size)> --l1-assoc <log2(ways)>"
                 " --l1-cyc <num> --l2-size <log2(size)> --l2-assoc <log2(ways)>"
                 " --l2-cyc <num>\n";
    std::exit(1);
}

int main(int argc, char** argv) {
    if (argc != 17) usage_and_exit(argv[0]);

    // Variables to hold parsed parameters
    std::string trace_file;
    unsigned mem_cyc = 0;
    int bsize      = -1;
    int wr_alloc   = -1;
    int l1_size    = -1, l1_assoc = -1, l1_cyc = -1;
    int l2_size    = -1, l2_assoc = -1, l2_cyc = -1;

    // First argument is trace file name
    trace_file = argv[1];
    // Parse flags in pairs: flag name + value
    for (int i = 2; i < argc; i += 2) {
        if      (!strcmp(argv[i], "--mem-cyc")) mem_cyc = std::stoi(argv[i+1]);
        else if (!strcmp(argv[i], "--bsize"))   bsize   = std::stoi(argv[i+1]);
        else if (!strcmp(argv[i], "--wr-alloc")) wr_alloc = std::stoi(argv[i+1]);
        else if (!strcmp(argv[i], "--l1-size"))  l1_size  = std::stoi(argv[i+1]);
        else if (!strcmp(argv[i], "--l1-assoc")) l1_assoc = std::stoi(argv[i+1]);
        else if (!strcmp(argv[i], "--l1-cyc"))   l1_cyc   = std::stoi(argv[i+1]);
        else if (!strcmp(argv[i], "--l2-size"))  l2_size  = std::stoi(argv[i+1]);
        else if (!strcmp(argv[i], "--l2-assoc")) l2_assoc = std::stoi(argv[i+1]);
        else if (!strcmp(argv[i], "--l2-cyc"))   l2_cyc   = std::stoi(argv[i+1]);
        else usage_and_exit(argv[0]);
    }
    // Validate presence of all flags
    if (bsize < 0 || wr_alloc < 0 || l1_size < 0 || l1_assoc < 0
     || l1_cyc < 0 || l2_size < 0 || l2_assoc < 0 || l2_cyc < 0) {
        usage_and_exit(argv[0]);
    }

    // Convert log2 parameters into actual sizes/ways
    unsigned block_size = 1U << bsize;      // e.g., bsize=5 → block_size=32
    unsigned L1_bytes   = 1U << l1_size;     // e.g., l1_size=16 → L1 size = 64 KB
    unsigned L1_ways    = 1U << l1_assoc;    // e.g., l1_assoc=3 → 8-way
    unsigned L2_bytes   = 1U << l2_size;
    unsigned L2_ways    = 1U << l2_assoc;

    // Create L1 and L2 cache objects
    CacheLevel* L1 = new CacheLevel(L1_bytes, block_size, L1_ways, l1_cyc);
    CacheLevel* L2 = new CacheLevel(L2_bytes, block_size, L2_ways, l2_cyc);

    // Create top-level Cache coordinator
    Cache topCache(L1, L2, mem_cyc, (wr_alloc == 1));

    // Open trace file for reading
    std::ifstream fin(trace_file);
    if (!fin) {
        std::cerr << "Error: cannot open trace file " << trace_file << "\n";
        return 1;
    }

    // Iterate through each line: 'r 0xADDRESS' or 'w 0xADDRESS'
    char op;
    std::string hexaddr;
    while (fin >> op >> hexaddr) {
        unsigned addr = std::stoul(hexaddr, nullptr, 16); // convert hex to unsigned
        bool is_write = (op == 'w');
        topCache.access(is_write, addr);
    }
    fin.close();

    // Compute statistics: L1 miss rate, L2 miss rate, average access time
    double L1miss_rate = double(topCache.l1_misses) / topCache.total_accesses;
    double L2miss_rate = 0.0;
    if (topCache.l1_misses > 0) {
        L2miss_rate = double(topCache.l2_misses) / topCache.l1_misses;
    }
    double avg_time = double(topCache.total_access_time) / topCache.total_accesses;

    

    // Print final result in exact format
    std::cout << "L1miss=" << std::fixed << std::setprecision(3)
              << L1miss_rate
              << " L2miss=" << L2miss_rate
              << " AccTimeAvg=" << avg_time
              << "";

    return 0;
}

/*
TODO:
1 ) Delete main, use CachSim file instead, 
remember to make the conversion size->2^size when calling Cache

2 ) Check for writing back dirty bits before eviction for all special cases
using chat's

3 ) Should have a single write function that takes into account the allocation.

4 ) On write-allocate we pull the old information along all levels but apply update (writing)
    only to highest level

5 ) Use CacheSim.cpp instead of main

6 ) check if get_index and get_tag should be public or private
*/
