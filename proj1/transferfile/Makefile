CFLAGS := -Wall --std=gnu99 -g -Wno-format-security

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread
endif

all: server client

server: transferserver.o
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

client: transferclient.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) 

.PHONY: clean

clean:
	rm -fr *.o server client
