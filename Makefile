CC = gcc
CFLAGS = -I. -Werror=return-type
LDLIBS = -lpthread

TARGETS = vport vswitch

all: $(TARGETS)

vport: vport.o tap_utils.o
    $(CC) -o $@ $^ $(LDLIBS)

vswitch: vswitch.o
    $(CC) -o $@ $^ $(LDLIBS)

%.o: %.c utils.h
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
