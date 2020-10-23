PROGRAM := telink_gateway
OBJDIR	:= obj
SRCEXTS := .c
CFLAGS += -g 
CFLAGS += -DDEBUG 
#CFLAGS += -errchk=longptr64 
#CFLAGS += -Wno-int-to-pointer-cast 
#CFLAGS += -Wno-pointer-to-int-cast
LDFLAGS += -lm
LDFLAGS += -lpthread
LDFLAGS += -lpaho-mqtt3a
RM = rm -rf
CC = gcc

SOURCES = $(shell find ./ -maxdepth 1 -name "*$(SRCEXTS)")
OBJS = $(foreach x,$(SRCEXTS), $(patsubst ./%$(x),$(OBJDIR)%.o,$(filter %$(x),$(SOURCES))))
OBJDIRS	= $(sort $(dir $(OBJS)))

.PHONY : all clean docs

all : $(PROGRAM)

$(OBJDIR)/main.o : main.c
	$(CC) -o $@ -c $(CFLAGS) $<
$(OBJDIR)/%.o : %$(SRCEXTS) %.h
	$(CC) -o $@ -c $(CFLAGS) $<
$(PROGRAM) : $(OBJS)
	$(CC) -o $(PROGRAM) $(OBJS) $(LDFLAGS)

clean :
	$(RM) $(OBJDIR)/* $(PROGRAM)
