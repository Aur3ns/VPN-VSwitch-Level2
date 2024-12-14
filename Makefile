CC = gcc
CFLAGS = -I. -Wall -Werror=return-type
LDLIBS = -lpthread

TARGETS = vport

all: $(TARGETS)

vport: vport.o
	$(CC) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) *.o

setup:
	@echo "Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y build-essential python3 python3-pip
	pip3 install -r requirements.txt

deploy: all
	@echo "Project compiled and ready to deploy."
