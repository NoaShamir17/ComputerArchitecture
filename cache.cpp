#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>


#define WRITE_ALLOCATE true
#define NO_WRITE_ALLOCATE false

class CacheLevel {
public:
    static unsigned current_time; // Static variable to track the current time for LRU replacement policy
    static unsigned total_acc; // Static variable to track total accesses for statistics
    static unsigned total_acc_time;  // Static variable to track total Access time for statistics

    unsigned size;        // Cache size in bytes
    unsigned block_size;   // Block size in bytes
    unsigned assoc;       // Associativity (log2 of number of ways)
    bool write_allocate; // Write allocate policy
    unsigned access_time;      // Access cycles for this cache level
    unsigned mem_time;     // Access cycles for memory
    double miss_rate;    // Miss rate for this cache level
    class Block **cache; // Pointer to the cache structure (access block with cache[set][way])
    
    CacheLevel(unsigned size, unsigned block_size, unsigned assoc,
               bool write_allocate, unsigned access_time, unsigned mem_time) {
        // Initialize cache structure here
    }

    ~CacheLevel() {
        // Clean up cache structure here if necessary
    }

    bool hit_check(unsigned tag, unsigned set) {
        // Check if the block with the given tag is present in the specified set
        // Return true if hit, false otherwise
    }
    void load_block(unsigned tag, unsigned set) {
        // Load the block with the given tag into the specified set
        // If necessary, evict a block using LRU policy
    }
    void evict_block(unsigned set) {
        // Evict a block from the specified set using LRU policy
        // Update the dirty bit if necessary
    }

    void read(unsigned tag, unsigned set) {
        // Read operation: check for hit/miss and update cache accordingly
        // If miss, load the block from memory
    }
    void write(unsigned tag, unsigned set) {
        // Write operation: check for hit/miss and update cache accordingly
        // If miss, load the block from memory if write-allocate is enabled
        // If not, just update the dirty bit if write-allocate is disabled
    }
};

class Block {
    public:
    unsigned tag;        // Tag for the block
    bool dirty;      // Dirty bit
    unsigned lastAccess; // Last access time for LRU replacement policy
}