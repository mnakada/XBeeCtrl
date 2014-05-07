#
# XbeeCtrl
#

target_program = $(shell pwd | sed 's!.*/!!')
cc_srcs = $(sort $(shell ls *.cc 2> /dev/null))
cc_header = $(sort $(shell ls *.h 2> /dev/null))
target_os = $(shell uname)
LIBS = 
CC_FLAGS = -I.

all: $(target_program)

$(target_program) : $(cc_srcs) $(cc_header)
	g++ $(CC_FLAGS) $(LIBS) -Wno-deprecated-declarations -o $@ $(cc_srcs)

clean:
	rm $(target_program)

install: all
	cp XBeeCtrl /usr/local/bin

