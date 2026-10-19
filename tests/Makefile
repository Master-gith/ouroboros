all:
	g++ -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined \
	  ../src/core/game.cpp ../src/core/profile.cpp test_core.cpp -o test_core
	./test_core
