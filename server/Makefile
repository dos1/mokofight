default:
	gcc -Wall -g main.c -o server
run: default
	./server
check:
	splint -warnposix -warnposixheaders -unrecog +matchanyintegral main.c
