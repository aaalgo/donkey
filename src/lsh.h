#ifndef WDONG_LSH
#define WDONG_LSH

#include <stdlib.h>
// this is a simple lsh library

namespace lsh {
    // struct DATA {
    //      typedef .. RECORD_TYPE;     // type of indexed record
    //      typedef .. QUERY_TYPE;
    //      typedef .. KEY_TYPE;
    //      static void hash (RECORD_TYPE const &, uint32_t *);
    //      static void hash (QUERY_TYPE const &, uint32_t *);
    //      static KEY_TYPE key (RECORD_TYPE const &);
    //      static float dist (RECORD_TYPE const &, QUERY_TYPE const &);
    // }

    template <typename Config>
    class Index {
        // A simple object to wrap malloc/free
        class Memory {
            size_t sz;
            void *ptr;
            static const size_t ALIGNMENT = 4096;
        public:
            Memory (size_t size): sz(size) {
                ptr = 0;
                int r = posix_memalign(&ptr, ALIGNMENT, size);
                BOOST_VERIFY(r == 0);
                BOOST_VERIFY(ptr);
            }
            ~Memory () {
                free(ptr);
            }
            void *get () {
                return ptr;
            }
            size_t size () const {
                return sz;
            }
        };

        // block of hash table
        struct Block {
            static const unsigned MAX = 31;
            uint32_t data[MAX];
            int32_t next;  // next bucket, -1 if this is the last bucket
        };

        struct Bucket { // a bucket is a linked list of blocks
                        // the last block might not be full, and the size is
                        // stored in "tail"
            int32_t first, last;    // first & last block, -1 for null, append-only link list of blocks
            uint32_t count;         // total records in the block
            uint32_t tail;          // # records in the last block
        };
        
        Memory memory;

        unsigned num_tables;
        unsigned hash_bits;
        unsigned table_size;
        std::vector<std::vector<Bucket>> tables;

        Block *blocks;
        typename Config::RECORD_TYPE *records;

        // Index memory usage:
        // * Feature storage
        // * Hash tables
        // * hash buckets

        size_t max_blocks;  // capacity
        size_t max_records;
        size_t n_records;   // already allocated
        size_t n_blocks;

        void addToBucket (Bucket *bucket, uint32_t i) {
            if (bucket->tail >= Block::MAX) {  // the last block is full
                // allocate new block
                if (n_blocks >= max_blocks) {
                    throw std::bad_alloc();
                }
                int n = n_blocks;
                ++n_blocks;
                blocks[bucket->last].next = n;  // add the chain
                bucket->last = n;
                blocks[bucket->last].next = -1;
                bucket->tail = 0;
            }
            blocks[bucket->last].data[bucket->tail] = i;
            ++bucket->tail;
            ++bucket->count;
        }

    public:
        size_t capacity () const {
            return max_records;
        }
        
        size_t size () const {
            return n_records;
        }

        Index (unsigned num_tables_, unsigned hash_bits_, size_t allocate) // allocate = # bytes to allocate
            : num_tables(num_tables_),
            hash_bits(hash_bits_),
            table_size(1 << hash_bits_),
            memory(allocate), tables(num_tables)
        {
            // compute capacities
            // allocate >= sizeof(RECORD_TYPE) * max_records  + sizeof(Block) * max_blocks
            // max_blocks = max_records / BLOCK_SIZE + NUM_TABLES * TABLE_SIZE
            size_t block_overhead = num_tables * table_size;
            size_t blocK_overhead_bytes = block_overhead * sizeof(Block);

            max_records = (memory.size() - blocK_overhead_bytes) * Block::MAX
                               / (sizeof(Block) * num_tables + sizeof(typename Config::RECORD_TYPE) * Block::MAX);
            
            max_blocks = block_overhead + max_records * num_tables / Block::MAX;

            size_t block_memory = max_blocks * sizeof(Block);
            size_t record_memory = max_records * sizeof(typename Config::RECORD_TYPE);
            BOOST_VERIFY(block_memory + record_memory <= memory.size());

            n_records = n_blocks = 0;

            blocks = reinterpret_cast<Block *>(memory.get());
            records = reinterpret_cast<typename Config::RECORD_TYPE *>(reinterpret_cast<char *>(memory.get()) + block_memory);

            // setup tables to be empty
            for (auto &table: tables) {
                table.resize(table_size);
                for (auto &e: table) {
                    e.first = e.last = n_blocks;
                    e.count = e.tail = 0;
                    blocks[e.last].next = -1;
                    ++n_blocks;
                }
            }
        }

        void append (typename Config::RECORD_TYPE const &rec) {
            try {
                if (n_records >= max_records) throw std::bad_alloc();
                int n = n_records;
                ++n_records;
                records[n] = rec;
                uint32_t hash[num_tables];
                Config::hash(rec, num_tables, hash_bits, hash);
                for (unsigned i = 0; i < num_tables; ++i) {
                    addToBucket(&tables[i][hash[i]],n);
                }
            }
            catch (std::bad_alloc const &) {
                throw;
            }
        }

        void search (typename Config::QUERY_TYPE const &query, float dist, std::vector<std::pair<typename Config::KEY_TYPE, float>> *keys, typename Config::SEARCH_PARAMS_TYPE const &params) {
            uint32_t hash[num_tables];
            Config::hash(query, num_tables, hash_bits, hash);
            // for each table
            for (unsigned i = 0; i < num_tables; ++i) {
                Bucket const &bucket = tables[i][hash[i]];
                // scan bucket
                int c = 0;
                int n = bucket.first;
                // n is the next block to search
                while (n >= 0) {
                    int m = (n == bucket.last) ? bucket.tail : Block::MAX;
                    Block const &block = blocks[n];
                    for (unsigned j = 0; j < m; ++j) { // scan the block
                        unsigned r = block.data[j];
                        float d = Config::dist(records[r], query, params);
                        bool good = false;
                        if (Config::POLARITY > 0) {
                            good = d >= dist;
                        }
                        else {
                            good = d <= dist;
                        }
                        if (good) {
                            keys->push_back(std::make_pair(Config::key(records[r]), d));
                        }
                        ++c;
                    }
                    n = block.next;
                }
                BOOST_VERIFY(c == bucket.count);
            }
        }

        void brutal (typename Config::QUERY_TYPE const &query, float dist, std::vector<typename Config::KEY_TYPE> *keys) {
            unsigned n = n_records; // as n_records is append only, we don't need to lock the index
            for (unsigned r = 0; r < n; ++r) {
                if (Config::dist(records[r], query) >= dist) {
                    keys->push_back(Config::key(records[r]));
                }
            }
        }
    };
}

#endif
