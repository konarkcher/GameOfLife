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

    Commander(const std::string& source)
            : nrow{GetRowCount(source)}, ncol{GetColCount(source)}, field(nrow, ncol) {
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
    }

    void Stop() {
    }

    void Quit() {
        Stop();
        // ...
    }

private:
    void InitiateGame() {
        int world_size;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        auto thread_count = static_cast<size_t> (world_size);

        size_t real_thread_count = std::min(thread_count - 1, nrow);
        size_t block_size = nrow / real_thread_count;

        /*
 *         for (size_t i = 0; i < real_thread_count - 1; ++i) {
 *                     threads_.emplace_back(&GameOfLife::ThreadCycle, this, block_size * i, block_size * (i + 1));
 *                             }
 *                                     threads_.emplace_back(&GameOfLife::ThreadCycle, this, block_size * (real_thread_count - 1), height);
 *                                             */
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
        return line.size();
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

class Computer {
};

int main() {
    MPI_Init(NULL, NULL);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank != 0) {
        Computer computer;
    } else {
        Commander* game = nullptr;

        while (true) {
            std::string query;
            std::cin >> query;

            if (query == "START") {
                std::string source;
                std::cin >> source;

                if (game) {
                    std::cout << "THE GAME HAS ALREADY STARTED\n";
                    continue;
                }

                if (source == "RANDOM") {
                    size_t height, width;
                    std::cin >> height >> width;

                    game = new Commander(height, width);
                } else {
                    game = new Commander(source);
                }
                continue;
            }
            if (query == "STATUS") {
                if (!game) {
                    std::cout << "START THE GAME FIRSTLY\n";
                    continue;
                }
                if (!game->RequestStatus()) {
                    std::cout << "STOP THE GAME FIRSTLY\n";
                }
                continue;
            }
            if (query == "RUN") {
                if (!game) {
                    std::cout << "START THE GAME FIRSTLY\n";
                    continue;
                }

                size_t iteration_count;
                std::cin >> iteration_count;
                game->Run(iteration_count);
                continue;
            }
            if (query == "STOP") {
                if (!game) {
                    std::cout << "START THE GAME FIRSTLY\n";
                    continue;
                }
                game->Stop();
                continue;
            }
            if (query == "QUIT") {
                QuitGame(game, true);
                continue;
            }
            if (query == "END") {
                QuitGame(game, false);
                break;
            }
            std::cout << "UNKNOWN COMMAND\n";
        }
    }

    MPI_Finalize();
    return 0;
}
