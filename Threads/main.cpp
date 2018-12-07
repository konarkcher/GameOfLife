#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace tpcc {
namespace solutions {

class CyclicBarrier {
public:
    explicit CyclicBarrier(const size_t thread_count)
            : all_thread_count_(thread_count) {
    }

    CyclicBarrier(const CyclicBarrier&) = delete;

    CyclicBarrier(CyclicBarrier&&) = delete;

    bool PassThrough() { // returns if thread was last
        const size_t phase = current_phase_;
        std::unique_lock<std::mutex> lock{mutex_[phase]};

        if (++arrived_thread_count_ == all_thread_count_) {
            current_phase_ = 1 - current_phase_;
            arrived_thread_count_ = 0;

            all_threads_arrived_[phase].notify_all();
            return true;
        } else {
            all_threads_arrived_[phase].wait(lock, [=] { return phase != current_phase_; });
            return false;
        }
    }

private:
    const size_t all_thread_count_;
    size_t arrived_thread_count_{0};
    size_t current_phase_{0};

    std::mutex mutex_[2];
    std::condition_variable all_threads_arrived_[2];
};

} // namespace solutions
} // namespace tpcc


template<class T>
const T& CyclicGet(const std::vector<T>& vec, const long index) {
    return vec[(index + vec.size()) % vec.size()];
}

template<class T>
T& CyclicGet(std::vector<T>& vec, const long index) {
    return vec[(index + vec.size()) % vec.size()];
}

class GameOfLife {
public:
    typedef std::vector<std::vector<bool>> Field;

    GameOfLife(const size_t thread_count, const size_t height, const size_t width) {
        Field& start_field = GetCurrentField();
        Field& next_field = GetNextField();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::bernoulli_distribution bern(0.5);

        for (size_t i = 0; i < height; ++i) {
            start_field.emplace_back(width);
            for (size_t j = 0; j < width; ++j) {
                start_field[i][j] = bern(gen);
            }
            next_field.emplace_back(width);
        }

        InitiateGame(height, thread_count);
    }

    GameOfLife(const size_t thread_count, const std::string& source) {
        std::ifstream in;
        in.open(source);

        std::string line;
        Field& start_field = GetCurrentField();
        Field& next_field = GetNextField();
        while (in >> line) {
            start_field.emplace_back();
            for (size_t i = 0; i < line.size(); i += 2) {
                start_field.back().push_back(line[i] == '1');
            }
            next_field.emplace_back(start_field.back().size());
        }

        InitiateGame(start_field[0].size(), thread_count);
    }

    bool RequestStatus() {
        if (required_iter_.load() != done_iter_.load()) {
            return false;
        }
        PrintStatus();
        return true;
    }

    void Run(const size_t iteration_count) {
        std::lock_guard lock{change_iterations_};

        required_iter_.fetch_add(iteration_count);
        can_iterate_.notify_one();
    }

    void Stop() {
        std::lock_guard lock{change_iterations_};
        required_iter_.store(done_iter_);
    }

    void Quit() {
        change_iterations_.lock();

        quit_ = true;
        can_iterate_.notify_one();

        change_iterations_.unlock();
        for (auto& thread: threads_) {
            thread.join();
        }
    }

private:
    void InitiateGame(const size_t height, const size_t thread_count) {
        size_t real_thread_count = std::min(thread_count, height);
        size_t block_size = height / real_thread_count;

        barrier_ = new tpcc::solutions::CyclicBarrier{real_thread_count};

        for (size_t i = 0; i < real_thread_count - 1; ++i) {
            threads_.emplace_back(&GameOfLife::ThreadCycle, this, block_size * i, block_size * (i + 1));
        }
        threads_.emplace_back(&GameOfLife::ThreadCycle, this, block_size * (real_thread_count - 1), height);
    }

    void ThreadCycle(size_t from, size_t to) {
        size_t local_done = 0;

        while (true) {
            ComputePiece(from, to);
            bool is_last = barrier_->PassThrough();

            if (is_last) {
                std::unique_lock lock{change_iterations_};
                can_iterate_.wait(lock, [this] { return required_iter_.load() > done_iter_.load() || quit_; });

                if (quit_) {
                    return;
                }
                done_iter_.fetch_add(1);

                if (verbose_) {
                    PrintStatus();
                }
            } else {
                while (!(done_iter_ > local_done || quit_)) {}

                if (quit_) {
                    return;
                }
            }
            ++local_done;
        }
    }

    void ComputePiece(size_t from, size_t to) {
        const Field& cur_field = GetCurrentField();
        Field& next_field = GetNextField();

        for (size_t i = 0; i < cur_field.size(); ++i) {
            for (size_t j = 0; j < cur_field[0].size(); ++j) {
                size_t alive_count = CountAlive(cur_field, i, j);

                if (cur_field[i][j]) {
                    next_field[i][j] = alive_count == 2 || alive_count == 3;
                } else {
                    next_field[i][j] = alive_count == 3;
                }
            }
        }
    }

    size_t CountAlive(const Field& field, size_t i, size_t j) {
        size_t count = 0;
        for (int vshift = -1; vshift < 2; ++vshift) {
            for (int hshift = -1; hshift < 2; ++hshift) {
                count += static_cast<int> (CyclicGet(CyclicGet(field, i + vshift), j + hshift));
            }
        }
        return count - static_cast<int> (field[i][j]);
    }

    void PrintStatus() {
        std::cout << "Done " << done_iter_ << " iteration(s). Current field:\n";
        PrintField();
    }

    void PrintField() {
        for (const auto& line: GetCurrentField()) {
            for (auto cell: line) {
                std::cout << (cell ? "\u2B1B" : "\u2B1C");
            }
            std::cout << '\n';
        }
    }

    Field& GetCurrentField() {
        return fields_[done_iter_.load() % 2];
    }

    Field& GetNextField() {
        return fields_[1 - done_iter_.load() % 2];
    }

    std::vector<std::thread> threads_;
    std::vector<Field> fields_{2};

    std::mutex change_iterations_;
    std::condition_variable can_iterate_;
    tpcc::solutions::CyclicBarrier* barrier_{nullptr};

    std::atomic<size_t> required_iter_{0}, done_iter_{0};
    bool verbose_{false}; // for debug purposes, non accessible from outside
    bool quit_{false};
};

void QuitGame(GameOfLife*& game, bool verbose) {
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

int main() {
    GameOfLife* game = nullptr;

    while (true) {
        std::string query;
        std::cin >> query;

        if (query == "START") {
            size_t thread_count;
            std::string source;
            std::cin >> thread_count >> source;

            if (game) {
                std::cout << "THE GAME HAS ALREADY STARTED\n";
                continue;
            }

            if (source == "RANDOM") {
                size_t height, width;
                std::cin >> height >> width;

                game = new GameOfLife(thread_count, height, width);
            } else {
                game = new GameOfLife(thread_count, source);
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

    return 0;
}