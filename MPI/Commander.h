#pragma once

#include <iostream>
#include <random>
#include <fstream>
#include <mpi.h>

#include "ContigousArray.h"

class Commander {
public:
    typedef ContigousArray<char> Field;

    Commander(const size_t height, const size_t width)
            : nrow_{height}, ncol_{width}, field_(nrow_, ncol_) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::bernoulli_distribution bern(0.5);

        for (size_t i = 0; i < height; ++i) {
            for (size_t j = 0; j < width; ++j) {
                field_[i][j] = bern(gen);
            }
        }

        InitiateGame();
    }

    explicit Commander(const std::string& source)
            : nrow_{GetRowCount(source)}, ncol_{GetColCount(source)}, field_(nrow_, ncol_) {
        std::cout << "nrow: " << nrow_ << ", ncol: " << ncol_ << '\n';

        std::ifstream in;
        in.open(source);

        std::string line;
        for (size_t i = 0; i < nrow_; ++i) {
            in >> line;
            for (size_t j = 0; j < line.size(); j += 2) {
                field_[i][j] = line[j];
                std::cout << field_[i][j];
            }
            std::cout << '\n';
        }

        InitiateGame();
    }

    bool RequestStatus() {
        if (!game_stopped_) {
            return false;
        }
        PrintStatus();
        return true;
    }

    void Run(const size_t iteration_count) {
        required_iter_ += iteration_count;
        NotifyAll('r');
        SendIterations();
        game_stopped_ = false;
    }

    void Stop() {
        NotifyAll('s');
        required_iter_ = 0;
        for (size_t i = 0; i < real_thread_count_; ++i) {
            unsigned long cur_iter;
            MPI_Recv(&cur_iter, 1, MPI_UNSIGNED_LONG, i + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            required_iter_ = std::max(required_iter_, cur_iter);
        }
        SendIterations();

        size_t block_size = nrow_ / real_thread_count_;
        size_t last_start = (real_thread_count_ - 1) * block_size;

        for (size_t i = 0; i < real_thread_count_ - 1; ++i) {
            MPI_Recv(field_.array[block_size * i], block_size * ncol_, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
        }
        MPI_Recv(field_.array[last_start], (nrow_ - last_start) * ncol_, MPI_CHAR, real_thread_count_, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        game_stopped_ = true;
    }

    void Quit() {
        NotifyAll('q');
    }

private:
    void InitiateGame() {
        int world_size;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        auto thread_count = static_cast<size_t> (world_size) - 1;

        real_thread_count_ = std::min(thread_count, nrow_);
        size_t block_size = nrow_ / real_thread_count_;
        size_t last_start = (real_thread_count_ - 1) * block_size;

        NotifyAll('c');

        char finalize_command = 'f';
        for (size_t i = real_thread_count_; i < thread_count; ++i) {
            MPI_Send(&finalize_command, 1, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD);
        }

        /*
        for (size_t i = 0; i < real_thread_count_ - 1; ++i) {
            unsigned long size[2] = {block_size, ncol_};
            MPI_Send(size, 2, MPI_UNSIGNED_LONG, i + 1, 0, MPI_COMM_WORLD);
            MPI_Send(field_.array[block_size * i], block_size * ncol_, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD);
        }
        unsigned long size[2] = {nrow_ - last_start, ncol_};
        MPI_Send(size, 2, MPI_UNSIGNED_LONG, real_thread_count_, 0, MPI_COMM_WORLD);
        MPI_Send(field_.array[last_start], (nrow_ - last_start) * ncol_, MPI_CHAR, real_thread_count_, 0,
                 MPI_COMM_WORLD);
        */
    }

    size_t GetRowCount(const std::string& source) {
        size_t nrow = 0;

        std::ifstream in;
        in.open(source);

        std::string line;
        while (in >> line) {
            ++nrow;
        }
        return nrow;
    }

    size_t GetColCount(const std::string& source) {
        std::ifstream in;
        in.open(source);

        std::string line;
        in >> line;
        return (line.size() + 1) / 2;
    }

    void NotifyAll(char command) {
        for (size_t i = 0; i < real_thread_count_; ++i) {
            MPI_Send(&command, 1, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD);
        }
    }

    void SendIterations() {
        for (size_t i = 0; i < real_thread_count_; ++i) {
            MPI_Send(&required_iter_, 1, MPI_UNSIGNED_LONG, i + 1, 0, MPI_COMM_WORLD);
        }
    }

    void PrintStatus() {
        std::cout << "Done " << required_iter_ << " iteration(s). Current field:\n";
        PrintField();
    }

    void PrintField() {
        for (size_t i = 0; i < nrow_; ++i) {
            for (size_t j = 0; j < ncol_; ++j) {
                // std::cout << (field_[i][j] == '1' ? "\u2B1B" : "\u2B1C");
                std::cout << field_[i][j];
            }
            std::cout << '\n';
        }
        for (size_t i = 0; i < nrow_; ++i) {
            for (size_t j = 0; j < ncol_; ++j) {
                std::cout << '1';
            }
            std::cout << '\n';
        }
    }

    unsigned long required_iter_{0};
    size_t real_thread_count_{0};
    size_t nrow_, ncol_;
    bool game_stopped_{true};

    Field field_;
};

void QuitGame(Commander*& game, bool verbose) {
    if (!game) {
        if (verbose) {
            std::cout << "START THE GAME FIRSTLY\n";
        }
        return;
    }
    game->Quit();

    delete game;
    game = nullptr;
}