suggested: suggested.c
	gcc -O0 -ggdb -o suggested suggested.c -levent -lsenna

clean:
	rm suggested *.o
