APP = fop

CFLAGS = -g -O3
CC = gcc

SRCS = $(APP).c
HDRS = 

#################################################

all: $(APP)

$(APP): $(SRCS:.c=.o)
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

%.o: %.c $(HDRS)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

clean:
	rm -f *.o *~ $(APP)
