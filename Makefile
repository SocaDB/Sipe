all:
	metil_comp -Isrc -g3 src/Sipe/sipe.cpp -e tests/http.sipe.py
#  -di -ds -ws
val:
	metil_comp --valgrind -Isrc -g3 src/Sipe/sipe.cpp -ws tests/http.sipe.py
#  --exec-using "valgrind --leak-check=full"
# metil_comp --exec-using "valgrind --leak-check=full" -Isrc -g3 src/Sipe/sipe.cpp tests/http.sipe
