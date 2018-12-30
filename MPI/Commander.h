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
            : nrow{height}, ncol{width}, field(nrow, ncol) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::bernoulli_distribution bern(0.5);

        for (size_t i = 0; i < height; ++i) {
            for (size_t j = 0; j < width; ++j) {
                field[i][j] = bern(gen);
            }
        }

        InitiateGame();
    }

    explicit Commander(const std::string& source)
            : nrow{GetRowCount(source)}, ncol{GetColCount(source)}, field(nrow, ncol) {
        std::cout << "nrow: " << nrow << ", ncol: " << ncol << '\n';


        std::ifstream in;
        in.open(source);

        std::string line;
        for (size_t i = 0; i < nrow; ++i) {
            in >> line;
            for (size_t j = 0; j < ncol; ++j) {
                field[i][j] = line[j];
            }
        }

        InitiateGame();
    }

    bool RequestStatus() {
        if (!game_stopped) {
            return false;
        }
        PrintStatus();
        return true;
    }

    void Run(const size_t iteration_count) {
        NotifyAll('r');
    }

    void Stop() {
        NotifyAll('s');
    }

    void Quit() {
        Stop();
        NotifyAll('q');
    }

private:
    void InitiateGame() {
        int world_size;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        auto thread_count = static_cast<size_t> (world_size) - 1;

        real_thread_count = std::min(thread_count, nrow);
        size_t block_size = nrow / real_thread_count;
        size_t last_start = (real_thread_count - 1) * block_size;

        NotifyAll('c');

        char finalize_command = 'f';
        for (size_t i = real_thread_count; i < thread_count; ++i) {
            MPI_Send(&finalize_command, 1, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD);
        }

        for (size_t i = 0; i < real_thread_count - 1; ++i) {
            std::cout << "Sending to rank " << i + 1 << " from " << block_size * i << ", size " << block_size * ncol << '\n';
            MPI_Send(field.array[block_size * i], block_size * ncol, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD);
        }
        std::cout << "Sending to rank " << real_thread_count << " from " << last_start << ", size " << (nrow - last_start) * ncol << '\n';
        MPI_Send(field.array[last_start], (nrow - last_start) * ncol, MPI_CHAR, real_thread_count, 0, MPI_COMM_WORLD);
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
        std::cout << "NCOL LINE: " << line << '\n';
        return line.size();
    }

    void NotifyAll(char command) {
        for (size_t i = 0; i < real_thread_count; ++i) {
            MPI_Send(&command, 1, MPI_CHAR, i + 1, 0, MPI_COMM_WORLD);
        }
    }

    void PrintStatus() {
        std::cout << "Done " << required_iter_ << " iteration(s). Current field:\n";
        PrintField();
    }

    void PrintField() {
        for (size_t i = 0; i < nrow; ++i) {
            for (size_t j = 0; j < ncol; ++j) {
                std::cout << (field[i][j] ? "\u2B1B" : "\u2B1C");
            }
            std::cout << '\n';
        }
    }

    size_t required_iter_{0};
    size_t real_thread_count{0};
    size_t nrow, ncol;
    bool game_stopped{true};

    Field field;
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