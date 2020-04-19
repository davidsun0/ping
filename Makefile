ping: ping.c
	$(CC) ping.c -o ping

.PHONY: clean
clean:
	rm ping

