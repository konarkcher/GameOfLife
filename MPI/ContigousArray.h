#pragma once

#include <cstddef>
#include <vector>

template<typename T>
class ContigousArray {
public:
    ContigousArray(size_t nrows, size_t ncols)
            : ncols_(ncols), array(nrows * ncols) {
    }

    T& Get(size_t i, size_t j = 0) {
        return array[i * ncols_ + j];
    }

    T& operator[](size_t i) {
        return Get(i);
    }

private:
    size_t ncols_;

    std::vector<T> array;
};
