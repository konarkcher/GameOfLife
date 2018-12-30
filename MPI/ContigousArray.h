#pragma once

#include <cstddef>
#include <vector>

template<typename T>
class ContigousArray {
public:
    ContigousArray(size_t nrows, size_t ncols)
            : pool(nrows * ncols), array(nrows) {
        for (unsigned i = 0; i < nrows; ++i) {
            array[i] = &pool[i * ncols];
        }
    }

    T* operator[](size_t index) {
        return array[index];
    }

private:
    std::vector<T> pool;
    std::vector<T*> array;
};