ping:
	$(CC) ping.c -o ping

.PHONY: clean
clean:
	rm ping

