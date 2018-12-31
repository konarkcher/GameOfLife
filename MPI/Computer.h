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

            std::cout << prev_ << ' ' << rank_ << ' ' << next_;

            std::vector<char> prev_field(ncol_), next_field(ncol_);
            if (rank_ % 2 == 0) {
                MPI_Send(field_->operator[](0), ncol_, MPI_CHAR, prev_, 0, MPI_COMM_WORLD);
                MPI_Recv(&prev_field[0], ncol_, MPI_CHAR, prev_, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                MPI_Send(field_->operator[](nrow_ - 1), ncol_, MPI_CHAR, next_, 1, MPI_COMM_WORLD);
                MPI_Recv(&next_field[0], ncol_, MPI_CHAR, next_, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else {
                if (rank_ == 1 && prev_ % 2 == 1) {
                    MPI_Send(field_->operator[](0), ncol_, MPI_CHAR, prev_, 0, MPI_COMM_WORLD);
                    MPI_Recv(&prev_field[0], ncol_, MPI_CHAR, prev_, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                } else {
                    MPI_Recv(&prev_field[0], ncol_, MPI_CHAR, prev_, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Send(field_->operator[](0), ncol_, MPI_CHAR, prev_, 0, MPI_COMM_WORLD);
                }

                MPI_Recv(&next_field[0], ncol_, MPI_CHAR, next_, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(field_->operator[](nrow_ - 1), ncol_, MPI_CHAR, next_, 1, MPI_COMM_WORLD);
            }

            std::cout << rank_ << ": ";
            for (size_t i = 0; i < ncol_; ++i) {
                std::cout << prev_field[i];
            }
            std::cout << '\n';

            for (size_t i = 0; i < nrow_; ++i) {
                std::cout << rank_ << ": ";
                for (size_t j = 0; j < ncol_; ++j) {
                    std::cout << field_->operator[](i)[j];
                }
                std::cout << '\n';
            }

            std::cout << rank_ << ": ";
            for (size_t i = 0; i < ncol_; ++i) {
                std::cout << next_field[i];
            }
            std::cout << '\n';


            auto updated_field = new Field(nrow_, ncol_);

            for (size_t i = 0; i < nrow_; ++i) {
                for (size_t j = 0; j < ncol_; ++j) {
                    size_t alive_count = CountAlive(prev_field, next_field, i, j);

                    if (field_->operator[](i)[j] == '1') {
                        updated_field->operator[](i)[j] = (alive_count == 2 || alive_count == 3 ? '1' : '0');
                    } else {
                        updated_field->operator[](i)[j] = (alive_count == 3 ? '1' : '0');
                    }
                }
            }

            delete field_;
            field_ = updated_field;
            ++done_iter_;
        }
    }

    void UpdateIterations() {
        MPI_Recv(&required_iter_, 1, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    size_t CountAlive(const std::vector<char>& prev_field, const std::vector<char>& next_field, size_t i, size_t j) {
        size_t count = 0;
        for (int vshift = -1; vshift < 2; ++vshift) {
            for (int hshift = -1; hshift < 2; ++hshift) {
                size_t hind = (j + hshift + ncol_) % ncol_;

                if (i == 0 && vshift == -1) {
                    count += static_cast<int> (prev_field[hind] == '1');
                } else if (i == nrow_ - 1 && vshift == 1) {
                    count += static_cast<int> (next_field[hind] == '1');
                } else {
                    count += static_cast<int> (field_->operator[](i + vshift)[hind] == '1');
                }
            }
        }
        return count - static_cast<int> (field_->operator[](i)[j] == '1');
    }

    bool field_required{false};

    size_t nrow_{0}, ncol_{0};
    unsigned long required_iter_{0}, done_iter_{0};
    int rank_, prev_{0}, next_{0};
    Field* field_ = nullptr;
};