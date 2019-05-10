/*
Copyright (c) 2016-2018, Xilinx, Inc.
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include "ap_int.h"
#include "hls/utils/x_hls_utils.h"
#include <assert.h>
#include "hls_stream.h"

namespace hls {

    /* Public domain code for JKISS RNG */
    static unsigned int x = 123456789,y = 987654321,z = 43219876,c = 6543217; /* Seed variables */
    static unsigned int JKISS() {
        unsigned long long t;
        x = 314527869 * x + 1234567;
        y ^= y << 5; y ^= y >> 7; y ^= y << 22;
        t = 4294584393ULL * z + c; c = t >> 32; z = t;
        return x + y + z;
    }

    template <int N>
    ap_uint<BitWidth<N-1>::Value> convert_from_one_hot(ap_uint<N> x) {
        int match_table[Power<2, N>::Value] = {};
#pragma HLS array_partition variable=match_table complete
        for(int i = 0; i < N; i++) {
            match_table[1 << i] = i;
        }
        return match_table[x];
    }

    // Populate matches based on the current keys.
    template <int SIZE, typename KeyT>
    void parallel_match(const KeyT &key, KeyT keys[SIZE], ap_uint<SIZE> &matches) {
#pragma HLS inline self off
#pragma HLS pipeline II=1
    parallel_match_loop:
        for(int i = 0; i < SIZE; i++) {
            matches[i] = (key == keys[i]);
        }
    }
    // Given an array of match flags, return true if any of the
    // flags is set.  In addition, output 'value' as the
    // corresponding element of tvalues.
    template <int SIZE, typename ValueT>
    struct selector {
        static bool parallel_select(ValueT &value,
                                    ap_uint<SIZE> tmatches, ValueT *tvalues/*[SIZE]*/) {
#pragma HLS pipeline II=1
            assert(SIZE > 4);
            ValueT newvalues[SIZE/4];
            ap_uint<SIZE/4> newmatches;
#pragma HLS array_partition variable=newvalues complete

        parallel_select_loop:
            for(int i = 0; i < SIZE/4; i++) {
                ap_uint<4> t = tmatches(4*i+3, 4*i);
                newmatches[i] = selector<4, ValueT>::parallel_select(newvalues[i],
                                                                     t,
                                                                     &tvalues[4*i]);
            }
            return selector<SIZE/4, ValueT>::parallel_select(value, newmatches, newvalues);
        }
    };
    // Given an array of match flags, return true if any of the
    // flags is set.  In addition, output 'value' as the
    // corresponding element of tvalues.
    template <int SIZE, typename ValueT>
    bool parallel_select_basecase(ValueT &value,
                                  ap_uint<SIZE> matches, ValueT *tvalues/*[SIZE]*/) {
        // FIXME: check validity...
        int i = convert_from_one_hot(matches);
        value = tvalues[i];
        return matches != 0;
    }
    template <typename ValueT>
    struct selector<4, ValueT> {
        static bool parallel_select(ValueT &value,
                                    ap_uint<4> tmatches, ValueT *tvalues/*[4]*/) {
#pragma HLS pipeline II=1
            return parallel_select_basecase(value, tmatches, tvalues);
        }
    };
    template <typename ValueT>
    struct selector<3, ValueT> {
        static bool parallel_select(ValueT &value,
                                    ap_uint<3> tmatches, ValueT *tvalues/*[3]*/) {
#pragma HLS pipeline II=1
            return parallel_select_basecase(value, tmatches, tvalues);
        }
    };
    template <typename ValueT>
    struct selector<2, ValueT> {
        static bool parallel_select(ValueT &value,
                                    ap_uint<2> tmatches, ValueT *tvalues/*[2]*/) {
#pragma HLS pipeline II=1
            return parallel_select_basecase(value, tmatches, tvalues);
        }
    };
    template <typename ValueT>
    struct selector<1, ValueT> {
        static bool parallel_select(ValueT &value,
                                    ap_uint<1> tmatches, ValueT *tvalues/*[1]*/) {
#pragma HLS pipeline II=1
            value = tvalues[0];
            return tmatches[0];
        }
    };

    template <int SIZE, typename KeyT, typename ValueT>
    class cam;

    template <int SIZE, typename KeyT, typename ValueT>
    std::ostream& operator<<(std::ostream& os, const cam<SIZE, KeyT, ValueT>& cam);

    template <int SIZE, typename KeyT, typename ValueT>
    class cam {
        typedef int HashT;
        KeyT keys[SIZE];
        ValueT values[SIZE];
        ap_uint<SIZE> valid;

    public:
        cam() {
            valid = 0;
            for(int i = 0; i < SIZE; i++) {
#pragma HLS unroll
                keys[i] = 0;
                // values[i] = 0;
            }
            #pragma HLS array_partition variable=keys complete
            #pragma HLS array_partition variable=values complete
            #pragma HLS reset variable=valid
        }
        void clear() {
            valid = 0;
        }
        // Retrieve the value associated with the given key in the cam.
        // Return true if there is such a value, or false if there is no such value.
        bool get(const KeyT &key, ValueT &value) {
            ap_uint<SIZE> matches;
            parallel_match(key, keys, matches);
            matches &= valid;
            return selector<SIZE, ValueT>::parallel_select(value, matches, values);
        }
        bool canInsert() {
            if(valid == ap_uint<SIZE>(-1)) {
                return false;
            } else {
                return true;
            }
        }
        // If there is space in the cam, add a new entry in the cam
        // with the given key and value.  If an entry
        // already exists with the given rkey, then it is replaced.
        // Return true if the operation succeeds or false if it fails.
        bool swap(const KeyT &rkey, const KeyT &key, const ValueT &value, bool add = true) {
            ap_uint<SIZE> matches;
            // Find the element, if any that matches.
            parallel_match(rkey, keys, matches);
            valid &= ~matches; // Mark matches as not valid
            //            matches &= valid;

            if(valid == ap_uint<SIZE>(-1)) {
                // Is the cam is full and we can't overwrite an existing element,
                // then we have to fail.
                return false;
            }

            // Capture the old values.
            KeyT oldkeys[SIZE];
            ValueT oldvalues[SIZE];
            ap_uint<SIZE> oldvalid;
            for(int i = 0; i < SIZE; i++) {
#pragma HLS unroll
                oldkeys[i] = keys[i];
                oldvalues[i] = values[i];
            }
            oldvalid = valid;

            // no match.  Insertion point is initial element.
            // Push other elements down by one.
            bool done = false;
            for(int i = 1; i < SIZE; i++) {
#pragma HLS unroll
                //                if(!oldvalid[i-1] || (oldvalid[i-1] && matches[i-1])) break;
                if(oldvalid[i-1]) {
                    keys[i] = oldkeys[i-1];
                    values[i] = oldvalues[i-1];
                    valid[i] = oldvalid[i-1];
                } else break;
            }
            keys[0] = key;
            values[0] = value;
            valid[0] = add;
            return true;
        }
        // If there is space in the cam, add a new entry in the cam
        // with the given key and value.  If an entry
        // already exists with the given key, then it is replaced.
        // Return true if the operation succeeds or false if it fails.
        bool insert(const KeyT &key, const ValueT &value) {
            return swap(key, key, value);
        }
        // Return a random key and value if one exists.
        // If this is possible, return true, else return false;
        bool sweep(KeyT &key, ValueT &value) {
// #pragma HLS dependence variable=keys inter false
// #pragma HLS dependence variable=values inter false
// #pragma HLS dependence variable=valid inter false
            // This uses the same shift logic as insertion
            if(!valid[SIZE-1]) {
                // for(int i = SIZE-1; i > 0; i--) {
                //     keys[i] = keys[i-1];
                //     values[i] = values[i-1];
                //     valid[i] = valid[i-1];
                // }
                // valid[0] = false;
                return false;
            } else {
                key = keys[SIZE-1];
                value = values[SIZE-1];
                return true;
            }
        }
        void shift() {
            if(!valid[SIZE-1]) {
                for(int i = SIZE-1; i > 0; i--) {
#pragma HLS unroll
                    keys[i] = keys[i-1];
                    values[i] = values[i-1];
                    valid[i] = valid[i-1];
                }
                valid[0] = false;
            }
        }

        // Remove the value associated with the given key in the cam.
        // Return true if there is such a value, or false if there is no such value.
        bool remove(const KeyT &key) {
            ap_uint<SIZE> matches;
            // Find the element, if any that matches.
            parallel_match(key, keys, matches);
            // Mark it as invalid.
            valid &= ~matches;
            return (matches != 0);
        }

        friend std::ostream& operator<< <SIZE, KeyT, ValueT>(std::ostream& os, const cam<SIZE, KeyT, ValueT>& cam);

    };

    template <int SIZE, typename KeyT, typename ValueT>
    std::ostream& operator<<(std::ostream& os, const cam<SIZE, KeyT, ValueT>& cam) {
         for(int i = 0; i < SIZE; i++) {
             os << i << ":";
             if(cam.valid[i]) {
                 os << cam.keys[i] << "->" << cam.values[i] << "\n";
             }
         }
         return os;
    }

    template <int SIZE, int FACTOR, typename KeyT, typename ValueT>
    class algorithmic_cam;

    template <int SIZE, int FACTOR, typename KeyT, typename ValueT>
    std::ostream& operator<<(std::ostream& os, const algorithmic_cam<SIZE, FACTOR, KeyT, ValueT>& cam);

    // SIZE is power of 2, FACTOR is power of 2.
    template <int SIZE, int FACTOR, typename KeyT, typename ValueT>
    class algorithmic_cam {
    public:
        const static int BANKSIZE = SIZE/FACTOR;
        const static int HASHES = FACTOR+1;
        const static int HASHBITS = BitWidth<BANKSIZE-1>::Value;
        const static int HASHESBITS = BitWidth<HASHES-1>::Value;
        const static int KEYBITS = Type_BitWidth<KeyT>::Value;

        typedef ap_uint<HASHBITS> HashT;
        typedef ap_uint<HASHESBITS> BankT;

        KeyT deleted_key;
        bool deleted_valid;
        cam<4, KeyT, ValueT> cache;
        KeyT hashes[HASHBITS][HASHES];
        KeyT mem_key[BANKSIZE][HASHES];
        ValueT mem_value[BANKSIZE][HASHES];
        bool mem_valid[BANKSIZE][HASHES];

        HashT hashfunction (const KeyT &key, int n) {
            HashT t;
            for(int i = 0; i < HASHBITS; i++) {
                // Basically, each bit of the hash is created by selecting
                // bits of the key, and then XORing them together.
                KeyT select = key & hashes[i][n];
                t[i] = select.xor_reduce();
            }
            assert(t>=0 && t<BANKSIZE);
            return t;
        }

        // // Given a key and a hash, return whether the key matches the hashed entry in bank n.
        // // In addition, return the value at that location.
        // bool lookup(const KeyT &key, const HashT &hash, int n, KeyT &storedkey, ValueT &storedvalue) {
        //     storedkey = mem_key[hash][n];
        //     value = mem_value[hash][n];
        //     return key = oldkey;
        // }
        // Given a key, hash it for each bank, lookup the corresponding values,
        // Figure out if the the key matches the corresponding stored key for the bank,
        // and if it is valid and return those flags as well.
        void lookup_all(const KeyT &key,
                        KeyT keys[HASHES],
                        HashT hashes[HASHES],
                        ValueT values[HASHES],
                        ap_uint<HASHES> &found,
                        ap_uint<HASHES> &valid) {
#pragma HLS array_partition variable=keys complete
#pragma HLS array_partition variable=hashes complete
#pragma HLS array_partition variable=values complete
#ifdef DEBUG
            std::cout << "lookup(" << key << "):\n";
#endif
            for(int i = 0; i < HASHES; i++) {
                HashT hash = hashfunction(key,i);
                hashes[i] = hash;
                KeyT storedkey = keys[i] = mem_key[hash][i];
                values[i] = mem_value[hash][i];
                found[i] = key == storedkey;
                valid[i] = mem_valid[hash][i];
#ifdef DEBUG
                std::cout << hashes[i] << " " << keys[i] << "->" << values[i] << " " << found[i] << " " << valid[i] << "\n";
#endif
            }
        }

        template <int N>
        ap_uint<N> random_permute(ap_uint<N> x) {
            for (int i = N - 1; i >= 1; i--) {
                /* 0 <= r <= i */
                int r = JKISS() % (i+1);
                bool temp = x[i];
                x[i] = x[r];
                x[r] = temp;
            }
            return x;
        }
        void init_hashes(KeyT hashes[HASHBITS][HASHES]) {
            // Each bit of the key is selected by 50% of the hash bits.
            // Each hash is randomly generated independently.
            ap_uint<KEYBITS> x=0;
            // Set half the bits of x.
            for(int i = 0; i < KEYBITS/2; i++) {
#pragma HLS unroll
                x[i] = true;
            }
            for(int i = 0; i < HASHBITS; i++) {

            	for(int j = 0; j < HASHES; j++) {
                    // randomly shuffle x.
                    x = random_permute(x);
                    hashes[i][j] = x;
                    // std::cout << "hashes[" << i << "][" << j << "]:"
                    //           << hashes[i][j].toString(2, false) << "\n";
                }
            }
        }
    public:
        algorithmic_cam() {
            init_hashes(hashes);
            #pragma HLS array_partition variable=hashes complete dim=0
            #pragma HLS array_partition variable=mem_key complete dim=2
            #pragma HLS array_partition variable=mem_value complete dim=2
            #pragma HLS array_partition variable=mem_valid complete dim=2
            #pragma HLS reset variable=mem_valid
            for(int i = 0; i < BANKSIZE; i++) {
                for(int j = 0; j < HASHES; j++) {
                    mem_valid[i][j] = false;
                    mem_key[i][j] = 0;
                    //-mem_value[i][j] = 0;
                }
            }
            deleted_valid = false;
        }
        void clear() {
            //            init_hashes(hashes);
            for(int i = 0; i < BANKSIZE; i++) {
                for(int j = 0; j < HASHES; j++) {
                    mem_valid[i][j] = false;
                }
            }
            cache.clear();
        }
        bool canInsert() {
            return cache.canInsert();
        }
        bool get(const KeyT &key, ValueT &value) {
            #pragma HLS pipeline
            KeyT keys[HASHES];
            HashT hashes[HASHES];
            ValueT values[HASHES];
            ap_uint<HASHES> found, valid;
            lookup_all(key, keys, hashes, values, found, valid);

            // Skip anything that's not valid.
            found &= valid;

            // FIXME: verify found should be one-hot.
            if(cache.get(key, value)) return true;
            return parallel_select_basecase(value, found, values);
        }

        // Given flags indicating whether the hash is found in each bank and valid in each bank,
        // select a bank to place the new key.  Return true if there is a hash collision on all
        // banks and the previous element at the given bank needs to be inserted somewhere else.
        bool pick_evict(ap_uint<HASHES> found, ap_uint<HASHES> valid, BankT &bank) {
            BankT find_open_bank_table[Power<2, HASHES>::Value] = {};
            for(int i = 0; i < Power<2, HASHES>::Value; i++) {
#pragma HLS unroll
                // This is really "countTrailingOnes"
                find_open_bank_table[i] = ap_uint<HASHES>(~i).reverse().countLeadingZeros();
            }
            ap_uint<HASHES> foundandvalid = found & valid;
            if(foundandvalid != 0) {
                // It already exists in the cache.
                bank = convert_from_one_hot(foundandvalid);
#ifdef DEBUG
                std::cout << "evicting " << foundandvalid << " from bank " << bank << "\n";
#endif
                assert(found[bank]);
                assert(valid[bank]);
                return false;
            } else if(valid == ap_uint<HASHES>(-1)) {
                assert(!found);
#ifdef DEBUG
                std::cout << "collision\n";
#endif
                // It doesn't exist and there is a hash collision on all banks.
                // This should probably be randomized?
                bank = 0;
                return true;
            } else {
                // It doesn't exist and there is an open bank.

#ifdef DEBUG
                std::cout << "open:" << valid << " " << find_open_bank_table[valid] << "\n";
#endif
                assert(!valid[find_open_bank_table[valid]]);
                bank = find_open_bank_table[valid];
                return false;
            }
        }
        bool sweep() {
            //            #pragma HLS inline self off
            //#pragma HLS pipeline II=1
        // #pragma HLS dependence variable=mem_key inter false
        // #pragma HLS dependence variable=mem_value inter false
        // #pragma HLS dependence variable=mem_valid inter false
            KeyT ikey;
            ValueT ivalue;
            bool ivalid;
            // select something out of the cam cache.
            ivalid = cache.sweep(ikey, ivalue);
            if(ivalid) {
                KeyT keys[HASHES];
                HashT hashes[HASHES];
                ValueT values[HASHES];

                ap_uint<HASHES> found, valid;
                lookup_all(ikey, keys, hashes, values, found, valid);

                // handle possible collisions
                BankT i;
                // If there was a collision then oldvalid is false and we exit the loop
                bool collision = pick_evict(found, valid, i); // ?
                assert(i <= HASHES);
                HashT hash = hashes[i];
                KeyT oldkey = keys[i];
                ValueT oldvalue = values[i];
                cache.swap(ikey, oldkey, oldvalue, collision); // The swap must happen with the following write.
                mem_key[hash][i] = ikey;
                mem_value[hash][i] = ivalue;
                mem_valid[hash][i] = true;
            } else {
                cache.shift();
            }
            return ivalid;
        }
        struct work {
            KeyT ikey;
            ValueT ivalue;
            KeyT oldkey;
            ValueT oldvalue;
            bool collision;
            HashT hash;
            BankT i;
            bool valid;
        };
        struct work work0, work1, work2, work3, work4, work5, work6, work7, work8;
        bool sweep1() {
            #pragma HLS pipeline II=1
            KeyT ikey;
            ValueT ivalue;
            bool ivalid;
            // select something out of the cam cache.
            work8 = work7;
            work7 = work6;
            work6 = work5;
            work5 = work4;
            work4 = work3;
            work3 = work2;
            work2 = work1;
            work1 = work0;
            work0.valid = false;
            if(work8.valid) {
                cache.swap(work8.ikey, work8.oldkey, work8.oldvalue, work8.collision); // The swap must happen with the following write.
                mem_key[work8.hash][work8.i] = work8.ikey;
                mem_value[work8.hash][work8.i] = work8.ivalue;
                mem_valid[work8.hash][work8.i] = true;
            } else {
            ivalid = cache.sweep(ikey, ivalue);
            if(ivalid) {
                KeyT keys[HASHES];
                HashT hashes[HASHES];
                ValueT values[HASHES];

                ap_uint<HASHES> found, valid;
                lookup_all(ikey, keys, hashes, values, found, valid);

                // handle possible collisions
                BankT i;
                // If there was a collision then oldvalid is false and we exit the loop
                bool collision = pick_evict(found, valid, i); // ?
                assert(i <= HASHES);
                HashT hash = hashes[i];
                KeyT oldkey = keys[i];
                ValueT oldvalue = values[i];
                work0.ikey = ikey;
                work0.ivalue = ivalue;
                work0.oldkey = oldkey;
                work0.oldvalue = oldvalue;
                work0.collision = collision;
                work0.hash = hash;
                work0.i = i;
                work0.valid = true;
            } else {
                //cache.shift();
            }
            }
 
            return ivalid;
        }
        /*  struct work {
            KeyT ikey;
            ValueT ivalue;
            KeyT oldkey;
            ValueT oldvalue;
            bool collision;
            HashT hash;
            BankT i;
            bool valid;
            };*/
        bool sweep3() {
            #pragma HLS pipeline II=1
            KeyT ikey;
            ValueT ivalue;
            bool ivalid;
            // select something out of the cam cache.
             static KeyT ikey0, ikey1, ikey2, ikey3, ikey4, ikey5, ikey6, ikey7, ikey8;
            ikey8 = ikey7;
            ikey7 = ikey6;
            ikey6 = ikey5;
            ikey5 = ikey4;
            ikey4 = ikey3;
            ikey3 = ikey2;
            ikey2 = ikey1;
            ikey1 = ikey0;

            static ValueT ivalue0, ivalue1, ivalue2, ivalue3, ivalue4, ivalue5, ivalue6, ivalue7, ivalue8;
          ivalue8 = ivalue7;
            ivalue7 = ivalue6;
            ivalue6 = ivalue5;
            ivalue5 = ivalue4;
            ivalue4 = ivalue3;
            ivalue3 = ivalue2;
            ivalue2 = ivalue1;
            ivalue1 = ivalue0;

            static KeyT oldkey0, oldkey1, oldkey2, oldkey3, oldkey4, oldkey5, oldkey6, oldkey7, oldkey8;
            oldkey8 = oldkey7;
            oldkey7 = oldkey6;
            oldkey6 = oldkey5;
            oldkey5 = oldkey4;
            oldkey4 = oldkey3;
            oldkey3 = oldkey2;
            oldkey2 = oldkey1;
            oldkey1 = oldkey0;

            static ValueT oldvalue0, oldvalue1, oldvalue2, oldvalue3, oldvalue4, oldvalue5, oldvalue6, oldvalue7, oldvalue8;
            oldvalue8 = oldvalue7;
            oldvalue7 = oldvalue6;
            oldvalue6 = oldvalue5;
            oldvalue5 = oldvalue4;
            oldvalue4 = oldvalue3;
            oldvalue3 = oldvalue2;
            oldvalue2 = oldvalue1;
            oldvalue1 = oldvalue0;
  
            static bool collision0, collision1, collision2, collision3, collision4, collision5, collision6, collision7, collision8;
            collision8 = collision7;
            collision7 = collision6;
            collision6 = collision5;
            collision5 = collision4;
            collision4 = collision3;
            collision3 = collision2;
            collision2 = collision1;
            collision1 = collision0;

            static ValueT hash0, hash1, hash2, hash3, hash4, hash5, hash6, hash7, hash8;
            hash8 = hash7;
            hash7 = hash6;
            hash6 = hash5;
            hash5 = hash4;
            hash4 = hash3;
            hash3 = hash2;
            hash2 = hash1;
            hash1 = hash0;

            static BankT i0, i1, i2, i3, i4, i5, i6, i7, i8;
            i8 = i7;
            i7 = i6;
            i6 = i5;
            i5 = i4;
            i4 = i3;
            i3 = i2;
            i2 = i1;
            i1 = i0;

            static bool valid0, valid1, valid2, valid3, valid4, valid5, valid6, valid7, valid8;
            valid8 = valid7;
            valid7 = valid6;
            valid6 = valid5;
            valid5 = valid4;
            valid4 = valid3;
            valid3 = valid2;
            valid2 = valid1;
            valid1 = valid0;

           valid0 = false;
           if(valid8) {
               cache.swap(ikey8, oldkey8, oldvalue8, collision8); // The swap must happen with the following write.
                mem_key[hash8][i8] = ikey8;
                mem_value[hash8][i8] = ivalue8;
                mem_valid[hash8][i8] = true;
            } else {
            ivalid = cache.sweep(ikey, ivalue);
            if(ivalid) {
                KeyT keys[HASHES];
                HashT hashes[HASHES];
                ValueT values[HASHES];

                ap_uint<HASHES> found, valid;
                lookup_all(ikey, keys, hashes, values, found, valid);

                // handle possible collisions
                BankT i;
                // If there was a collision then oldvalid is false and we exit the loop
                bool collision = pick_evict(found, valid, i); // ?
                assert(i <= HASHES);
                HashT hash = hashes[i];
                KeyT oldkey = keys[i];
                ValueT oldvalue = values[i];
                ikey0 = ikey;
                ivalue0 = ivalue;
                oldkey0 = oldkey;
                oldvalue0 = oldvalue;
                collision0 = collision;
                hash0 = hash;
                i0 = i;
                valid0 = true;
            } else {
                //cache.shift();
            }
            }
 
            return ivalid;
        }

        bool sweep2() {
        // #pragma HLS dependence variable=mem_key inter false
        // #pragma HLS dependence variable=mem_value inter false
        // #pragma HLS dependence variable=mem_valid inter false
            static KeyT ikey;
            static ValueT ivalue;
            static bool ivalid;
            static int state;

            static KeyT keys[HASHES];
            static HashT hashes[HASHES];
            static ValueT values[HASHES];
            static KeyT keys2[HASHES];
            static HashT hashes2[HASHES];
            static ValueT values2[HASHES];
#pragma HLS array_partition variable=keys2 complete
#pragma HLS array_partition variable=hashes2 complete
#pragma HLS array_partition variable=values2 complete

            static ap_uint<HASHES> found, valid;
            static ap_uint<HASHES> found2, valid2;
            // handle possible collisions
            static BankT bank;

            static BankT bank2;
            static BankT bank3;
            // If there was a collision then oldvalid is false and we exit the loop
            static bool collision;
            static HashT hash;
            static KeyT oldkey;
            static ValueT oldvalue;
            static bool deleting;
            BankT b;
            switch(state) {
            case 0:
                // select something out of the cam cache.
                ivalid = cache.sweep(ikey, ivalue);
                if(!ivalid) { ikey=deleted_key; ivalue = 0x0; deleting = deleted_valid;

                } else { deleting = false; }

#ifdef DEBUG
                if(ivalid) std::cout << "Sweep Select " << ikey << "->" << ivalue << "\n";
                if(deleting) std::cout << "Sweep Delete " << ikey;
#endif
                cache.shift();
                state = 1;
                break;
            case 1:
                if(ivalid || deleting) {
                    for(int i = 0; i < HASHES; i++) {
                        HashT hash = hashfunction(ikey,i);
                        hashes[i] = hash;
                    }
                //                    lookup_all(ikey, keys, hashes, values, found, valid);
#ifdef DEBUG
                    std::cout << "Sweep Lookup " << ikey << " is valid.\n";
#endif
                }
                state = 2;
                break;
            case 2:
                for(int i = 0; i < HASHES; i++) {
                    HashT hash = hashes[i];
                    KeyT storedkey =
                    keys2[i] = mem_key[hash][i];
                    values2[i] = mem_value[hash][i];
                    found2[i] = ikey == storedkey;
                    valid2[i] = mem_valid[hash][i];
                    hashes2[i] = hash;
                }
                // for(int i = 0; i < HASHES; i++) {
                //     keys2[i] = keys[i];
                //     hashes2[i] = hashes[i];
                //     values2[i] = values[i];
                // }
                // found2 = found;
                // valid2 = valid;
                state = 3;
                break;
            case 3:
                if(ivalid || deleting) {
                    collision = pick_evict(found2, valid2, b) && !deleting; // ?
                    bank = 4;
#ifdef DEBUG
                    std::cout << "Sweep Evict " << ikey << " to [" << bank << "][" << hashes2[bank] << "]";
                    if(collision) std::cout << " collides with " << keys2[bank] << "\n";
                    else std::cout << " no collision\n";
#endif
                    assert(bank <= HASHES);
                }
                state = 4;
                break;
            case 4:
                bank2 = bank;
                state = 5;
                break;
            case 5:
                bank3 = bank2;
                state = 6;
                break;
            case 6:
                if(ivalid || deleting) {
                    hash = hashes2[bank2];
                    KeyT oldkey = keys2[bank2];
                    ValueT oldvalue = values2[bank2];
                    bool b = cache.swap(ikey, oldkey, oldvalue, collision); // The swap must happen simultaneously with the following write.
                    mem_key[hash][bank2] = ikey;
                    mem_value[hash][bank2] = ivalue;
                    mem_valid[hash][bank2] = ivalid;
                    if(deleting) deleted_valid = false;
#ifdef DEBUG
                    std::cout << "Sweep Writeback " << ikey << " evicted " << oldkey << "->" << oldvalue << "\n";
                    std::cout << *this << "\n";
#endif
                }
                state = 0;
                break;
            }
            return ivalid;
        }
        bool insert(const KeyT &key, const ValueT &value) {
            return cache.insert(key, value);
        }
        bool insert_nocache(const KeyT &key, const ValueT &value) {
            KeyT ikey = key;
            ValueT ivalue = value;
            bool ivalid = true;

            while(ivalid) {
                #pragma HLS pipeline
                KeyT keys[HASHES];
                HashT hashes[HASHES];
                ValueT values[HASHES];

                ap_uint<HASHES> found, valid;
                lookup_all(ikey, keys, hashes, values, found, valid);

                // handle possible collisions
                BankT i;
                // If there was a collision then oldvalid is false and we exit the loop
                bool collision = pick_evict(found, valid, i); // ?
                assert(i <= HASHES);
                HashT hash = hashes[i];
                KeyT oldkey = keys[i];
                ValueT oldvalue = values[i];
                mem_key[hash][i] = ikey;
                mem_value[hash][i] = ivalue;
                mem_valid[hash][i] = true;
                ikey = oldkey;
                ivalue = oldvalue;
                ivalid = collision;
            }
            return true;
        }
        // Remove the value associated with the given key in the cache.
        // Return true if there is such a value, or false if there is no such value.
        bool remove(const KeyT &key) {
            //            bool b = cache.remove(key);
            if(deleted_valid) return false;
            deleted_key = key;
            deleted_valid = true;
            // KeyT keys[HASHES];
            // HashT hashes[HASHES];
            // ValueT values[HASHES];
            // ap_uint<HASHES> found, valid;
            // lookup_all(key, keys, hashes, values, found, valid);

            // ap_uint<HASHES> foundandvalid = found & valid;
            // if(foundandvalid != 0) {
            //     BankT i = convert_from_one_hot(foundandvalid);
            //     HashT hash = hashes[i];
            //     mem_valid[hash][i] = false;
            //     return true;
            // } else {
            //     return b;
            // }
            return true;
        }

        friend std::ostream& operator<< <SIZE, FACTOR, KeyT, ValueT>(std::ostream& os,
                                                                     const algorithmic_cam<SIZE, FACTOR, KeyT, ValueT>& cam);
        };

    template <int SIZE, int FACTOR, typename KeyT, typename ValueT>
    std::ostream& operator<<(std::ostream& os,
                             const algorithmic_cam<SIZE, FACTOR, KeyT, ValueT>& cam) {
        os << cam.cache;
        for(int i = 0; i < cam.BANKSIZE; i++) {
            for(int j = 0; j < cam.HASHES; j++) {
                if(cam.mem_valid[i][j]) {
                    os << "[" << i << "][" << j << "]:"
                       << cam.mem_key[i][j] << "->" << cam.mem_value[i][j] << "\n";
                }
            }
        }
        return os;
    }

template <int _N, typename _KeyT, typename _ValueT, typename _LookupSourceT = ap_uint<1>, typename _UpdateSourceT=ap_uint<1> >
class SmartCam {
public:
    typedef _KeyT KeyT;
    typedef _ValueT ValueT;
    typedef _LookupSourceT LookupSourceT;
    typedef _UpdateSourceT UpdateSourceT;
    static const int N = _N;

struct LookupRequest {
	KeyT		key;
	LookupSourceT			source;
	LookupRequest():key(0), source(0) {}
	LookupRequest(KeyT searchKey)
				:key(searchKey), source(0) {}
};

struct UpdateRequest {
	KeyT			key;
	ValueT			value;
    ap_uint<1>			op;
	UpdateSourceT			source;
	UpdateRequest(): key(0), value(0),op(0),source(0) {}
	UpdateRequest(KeyT key, ValueT value, ap_uint<1> op)
			:key(key), value(value), op(op), source(0) {}
};

struct LookupReply {
	ap_uint<1>			hit;
	ValueT			value;
	LookupSourceT			source;
	LookupReply():hit(0), value(0), source(0) {}
	LookupReply(bool hit, ValueT returnValue)
        :hit(hit), value(returnValue), source(0) {}
};

struct UpdateReply {
	ValueT			value;
	ap_uint<1>			op;
    UpdateSourceT		source;

	UpdateReply()
        :value(0), op(0), source(0){}
	UpdateReply(ap_uint<1> op)
        :value(0), op(op), source(0) {}
	UpdateReply(ValueT id, ap_uint<1> op)
        :value(id), op(op), source(0) {}
};
static void top(hls::stream<LookupRequest>	&LookupReq,
         hls::stream<LookupReply>		&LookupResp,
         hls::stream<UpdateRequest>	&UpdateReq,
         hls::stream<UpdateReply>		&UpdateResp) {
	#pragma	HLS INTERFACE axis port=LookupReq	 bundle=LookupReq
	#pragma	HLS INTERFACE axis port=LookupResp	 bundle=LookupResp
	#pragma	HLS INTERFACE axis port=UpdateReq	 bundle=UpdateReq
	#pragma	HLS INTERFACE axis port=UpdateResp	 bundle=UpdateResp
	#pragma HLS DATA_PACK variable=LookupReq
	#pragma HLS DATA_PACK variable=LookupResp
	#pragma HLS DATA_PACK variable=UpdateReq
	#pragma HLS DATA_PACK variable=UpdateResp

    static hls::algorithmic_cam<N, 4, KeyT, ValueT> mycam;

    // get_loop: for(int i = 0; i < 16; i++) {
#pragma HLS pipeline II=1
#pragma HLS inline all recursive

        if(!LookupReq.empty()) {
            LookupRequest req = LookupReq.read();
            KeyT k = req.key;
            ValueT v;
            bool b = mycam.get(k,v);
            LookupReply resp(b,v);
            resp.source = req.source;
            LookupResp.write(resp);
        } else
        if(!UpdateReq.empty() && mycam.canInsert()) {
            UpdateRequest req = UpdateReq.read();
            KeyT kin = req.key;
            ValueT vin = req.value;
            if(!req.op) {
                bool b = mycam.insert(kin, vin);
                assert("Cache is full" && b);
            } else {
                mycam.remove(kin);
            }
            UpdateReply resp(req.op);
            resp.source = req.source;
            UpdateResp.write(resp);
        }
        else
            mycam.sweep2();
     //    else
//             if(i%8) {
// #pragma HLS occurrence cycle=8
//             mycam.sweep();
//         }
//    }
        std::cout << mycam << "\n"; 
//  sweep_loop: for(int i = 0; i < 16; i++) {
//         #pragma HLS pipeline II=1
// // #pragma HLS dependence variable=mycam.mem_key false
// // #pragma HLS dependence variable=mycam.mem_value false
// // #pragma HLS dependence variable=mycam.mem_valid false

//         mycam.sweep3();
//  }
}

};
}

