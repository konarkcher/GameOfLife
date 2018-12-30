#pragma once

#include <iostream>

#include "ContigousArray.h"

class Computer {
public:
    typedef ContigousArray<char> Field;

    explicit Computer(const int world_rank)
            : rank_(world_rank) {
        int neighs[2];
        MPI_Recv(&neighs, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        prev_ = neighs[0], next_ = neighs[1];

        std::cout << prev_ << ' ' << rank_ << ' ' << next_ << '\n';

        unsigned long size[2];
        MPI_Recv(&size, 2, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        nrow_ = size[0], ncol_ = size[1];
        field_ = new Field(nrow_, ncol_);
        MPI_Recv(field_->operator[](0), nrow_ * ncol_, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

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
                    if (command == 'q') {
                        return;
                    } else if (command == 'r') {
                        UpdateIterations();
                    } else if (command == 's') {
                        field_required = true;
                        MPI_Send(&done_iter_, 1, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);
                        UpdateIterations();
                    } else {
                        std::cout << "UNKNOWN COMMAND " << command << " IN MAIN LOOP\n";
                    }
                }

                if (required_iter_ == done_iter_ && field_required) {
                    field_required = false;
                    MPI_Send(field_->operator[](0), nrow_ * ncol_, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
                }
            } while (required_iter_ == done_iter_);

            ++done_iter_;
        }
    }

    void UpdateIterations() {
        MPI_Recv(&required_iter_, 1, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    bool field_required{false};

    size_t nrow_{0}, ncol_{0};
    unsigned long required_iter_{0}, done_iter_{0};
    int rank_, prev_{0}, next_{0};
    Field* field_ = nullptr;
};