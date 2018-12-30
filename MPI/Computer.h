#pragma once

#include <iostream>

#include "ContigousArray.h"

class Computer {
public:
    typedef ContigousArray<char> Field;

    explicit Computer(const int world_rank)
            : rank(world_rank) {
        unsigned long size[2];
        MPI_Recv(&size, 2, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        nrow = size[0], ncol = size[1];
        field = new Field(nrow, ncol);
        MPI_Recv(field[0], nrow * ncol, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        StartMainLoop();
    }

private:
    void StartMainLoop() {
        while (true) {
            int message_available = 0;
            do {
                MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &message_available, MPI_STATUS_IGNORE);
                if (message_available) {
                    char command;
                    MPI_Recv(&command, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    std::cout << rank << ' ' << command << '\n';
                }
            } while (required_iter == done_iter);
        }
    }

    bool field_required{false};

    size_t nrow{0}, ncol{0};
    size_t required_iter{0}, done_iter{0};
    int rank;
    Field* field = nullptr;
};