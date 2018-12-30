#include "Commander.h"
#include "Computer.h"

int main() {
    MPI_Init(NULL, NULL);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank != 0) {
        char command;
        MPI_Recv(&command, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (command != 'f') {
            Computer computer(world_rank);
        }
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
