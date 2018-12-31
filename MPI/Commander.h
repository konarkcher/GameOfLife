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
                field_[i][j] = (bern(gen) ? '1' : '0');
            }
        }

        InitiateGame();
    }

    explicit Commander(const std::string& source)
            : nrow_{GetRowCount(source)}, ncol_{GetColCount(source)}, field_(nrow_, ncol_) {
        std::ifstream in;
        in.open(source);

        std::string line;
        for (size_t i = 0; i < nrow_; ++i) {
            in >> line;
            for (size_t j = 0; j * 2 < line.size(); ++j) {
                field_[i][j] = line[j * 2];
            }
        }

        InitiateGame();
    }

    bool RequestStatus() {
        unsigned long required_backup = required_iter_;
        Stop();
        if (required_iter_ != required_backup) {
            Run(required_backup - required_iter_);
        }

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
        std::cout << "NotifyAll('s');\n";
        NotifyAll('s');
        required_iter_ = 0;
        for (size_t i = 0; i < real_thread_count_; ++i) {
            unsigned long cur_iter;
            std::cout << "MPI_Recv(&cur_iter, 1, MPI_UNSIGNED_LONG, " << i + 1 << ", 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);\n";
            MPI_Recv(&cur_iter, 1, MPI_UNSIGNED_LONG, i + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            required_iter_ = std::max(required_iter_, cur_iter);
        }
        std::cout << "SendIterations();\n";
        SendIterations();

        size_t block_size = nrow_ / real_thread_count_;
        size_t last_start = (real_thread_count_ - 1) * block_size;

        for (size_t i = 0; i < real_thread_count_ - 1; ++i) {
            std::cout << "MPI_Recv(field_[block_size * i], block_size * ncol_, MPI_CHAR, " << i + 1 << ", 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);\n";
            MPI_Recv(field_[block_size * i], block_size * ncol_, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        std::cout << "MPI_Recv(field_[last_start], (nrow_ - last_start) * ncol_, MPI_CHAR, real_thread_count_, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);\n";
        MPI_Recv(field_[last_start], (nrow_ - last_start) * ncol_, MPI_CHAR, real_thread_count_, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
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

        for (size_t i = 0; i < real_thread_count_ - 1; ++i) {
            int neighs[2] = {static_cast<int> (i), static_cast<int> (i + 2)};
            if (neighs[0] == 0) {
                neighs[0] = static_cast<int> (real_thread_count_);
            }
            MPI_Send(neighs, 2, MPI_INT, i + 1, 0, MPI_COMM_WORLD);

            unsigned long size[2] = {block_size, ncol_};
            MPI_Send(size, 2, MPI_UNSIGNED_LONG, i + 1, 0, MPI_COMM_WORLD);
            MPI_Send(field_[block_size * i], block_size * ncol_, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD);
        }
        unsigned long size[2] = {nrow_ - last_start, ncol_};
        int neighs[2] = {static_cast<int> (real_thread_count_ - 1), 1};
        if (neighs[0] == 0) {
            neighs[0] = static_cast<int> (real_thread_count_);
        }

        MPI_Send(neighs, 2, MPI_INT, real_thread_count_, 0, MPI_COMM_WORLD);
        MPI_Send(size, 2, MPI_UNSIGNED_LONG, real_thread_count_, 0, MPI_COMM_WORLD);
        MPI_Send(field_[last_start], (nrow_ - last_start) * ncol_, MPI_CHAR, real_thread_count_, 0, MPI_COMM_WORLD);
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
                std::cout << field_[i][j];
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