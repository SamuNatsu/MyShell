all: main.c
	@echo "Building relase..."
	@gcc -O3 -Wall -o shell main.c -lreadline
	@echo "Done"
	@echo "Building debug..."
	@gcc -g -Wall -o shell_d main.c -lreadline
	@echo "Done"

clean:
	@if [ -e shell ]; then rm shell; fi
	@if [ -e shell_d ]; then rm shell_d; fi
	@echo "Done"
