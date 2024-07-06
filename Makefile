#
# Flags
#

CFLAGS= -O2 -I${INTEL_DIALOGIC_INC} -DLINUX -Wall -Wextra -std=c99 -g
SCFLAGS= -I${INTEL_DIALOGIC_DIR}/sctools
LFLAGS=-cu

#
# Programming Tools
#

CC=cc
LINT=lint

SCTOOLS_DIR= ./
SCTOOLS= $(SCTOOLS_DIR)sctools

SCTOOLS_SRC= ${INTEL_DIALOGIC_DIR}/sctools/sctools.c

#
# End of PTR# 14297 - SD
#
# Lint Files
#

ANSR_DIR= ./
CBANSR= $(ANSR_DIR)cbansr
CBANSR_ISDN= $(ANSR_DIR)cbansr_isdn
VER_DIR=./

DEMOS= $(CBANSR)

#
# Includes
#

VTINCLUDES= $(VTINCLUDE)digits.h $(VTINCLUDE)menu.h $(VTINCLUDE)date.h 


################################################################################
# Dependency Lines
################################################################################
all:	$(DEMOS)

#
# cbansr 
#
$(CBANSR): $(SCTOOLS).o $(CBANSR).o $(CBANSR_ISDN).o
	$(CC) -o$@ $@.o $(SCTOOLS).o $(CBANSR_ISDN).o -lrt -ldxxx -ldti -lsrl -lcurses -lgc -m32

$(CBANSR).o: $(CBANSR).c $(ANSR_DIR)cbansrx.h dispx.h
	cd $(@D) ; $(CC) $(CFLAGS) $(SCFLAGS) -c $(<F) -m32

$(CBANSR_ISDN).o: $(CBANSR_ISDN).c $(ANSR_DIR)cbansrx.h /usr/dialogic/inc/gcisdn.h /usr/dialogic/inc/gclib.h
	cd $(@D) ; $(CC) $(CFLAGS) $(SCFLAGS) -c $(<F) -m32 

$(CBANSR).ln: $(CBANSR).c $(ANSR_DIR)cbansrx.h
	cd  $(@D) ; $(LINT) $(LFLAGS) `basename $(CBANSR)`.c 
	$(LINT) -u $(CBANSR).ln -ld4xt

#$(CBANSR): $(DISPLAY).o  $(SCTOOLS).o $(CBANSR).o
#	$(CC) -o$@ $@.o $(SCTOOLS).o $(DISPLAY).o -ldxxx -ldti -lsrl

#$(CBANSR).o: $(CBANSR).c $(ANSR_DIR)cbansrx.h
#	cd $(@D) ; $(CC) $(CFLAGS) $(SCFLAGS) -c $(<F)

#$(CBANSR).ln: $(CBANSR).c $(ANSR_DIR)cbansrx.h
#	cd  $(@D) ; $(LINT) $(LFLAGS) `basename $(CBANSR)`.c 
#	$(LINT) -u $(CBANSR).ln -ld4xt

#
# sctools - Updated for PTR# 14297 - SD
#

$(SCTOOLS).o: $(SCTOOLS_SRC)
	cd $(SCTOOLS_DIR); $(CC) $(SCFLAGS) -DDTISC -c $(SCTOOLS_SRC) -m32

#
# Lint
#
lint:	$(LINTFILES) 

clean:
	-rm -f $(CBANSR)
	-rm -f $(CBANSR).o
	-rm -f $(SCTOOLS).o
	-rm -f $(CBANSR_ISDN).o
