#pragma once

#include <cstddef>

template<typename T>
class ContigousArray {
public:
    ContigousArray(size_t nrows, size_t ncols) {
        auto pool = new T[nrows * ncols];
        array = new T* [nrows];

        for (unsigned i = 0; i < nrows; ++i) {
            array[i] = &pool[i * ncols];
        }
    }

    T* operator[](size_t index) {
        return array[index];
    }

    ~ContigousArray() {
        delete[] array[0];  // remove the pool
        delete[] array;
    }

    T** array;
};
