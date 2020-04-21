all: compile

test: compile
	sudo ./ping -T 20 -w 2 www.cloudflare.com 

compile:
	gcc -o ping ./ping_cli.c

clean:
	rm ./ping
