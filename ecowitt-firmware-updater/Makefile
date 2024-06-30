ALL = ecowitt-firmware-updater

CFLAGS = -O -Wall
LDLIBS = -pthread

all: $(ALL)

clean:
	rm -f $(ALL)

ecowitt-firmware-updater: ecowitt-firmware-updater.c
	$(CC) $(CFLAGS) -o $@ ecowitt-firmware-updater.c $(LDFLAGS) $(LDLIBS)
