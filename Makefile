################################################################################
# CONFIG                                                                       #
################################################################################

COMPILER := cc
EXECUTABLE := progress
LIBS := z
CFLAGS := -g

################################################################################
# DO NOT CHANGE ANYTHING BELOW THIS LINE IF YOU DO NOT KNOW WHAT YOU ARE DOING #
################################################################################

SOURCE := progress.c

all : $(EXECUTABLE)

build : $(EXECUTABLE)

clean :
	rm -f $(EXECUTABLE)

install : $(EXECUTABLE)
	mkdir -p ${DESTDIR}/usr/bin/
	cp $(EXECUTABLE) ${DESTDIR}/usr/bin/

$(EXECUTABLE) :
	$(COMPILER) $(CFLAGS) $(SOURCE) -o $(EXECUTABLE) $(addprefix -l,$(LIBS))
	strip $(EXECUTABLE)
