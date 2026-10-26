#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdint>
#include <vector>
#include <pair>
#include <iostream>

//functions for getting big endian stuff within 8 bytes and storing them

//MSByte appears first

template<int N>                                 
uint64_t load_be(const char* p){
    static_assert(N>=1 && N<=8);  
    uint64_t v = 0;
    for(int i=0;i<N;i++){
        v = (v<<8)|static_cast<uint8_t>(p[i]);
    }
    return v;
}

template<int N>
void store_be(char* p, uint64_t v){
    static_assert(N>=1 && N<=8);
    for(int i=N-1;i>=0;i--){
        p[i] = static_cast<char>(v & 0xff);
        v = v>>8;
    }
}

#endif