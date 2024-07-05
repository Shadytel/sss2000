##########@@@SOFT@@@WARE@@@COPY@@@RIGHT@@@###################################
# DIALOGIC CONFIDENTIAL
#
# Copyright (C) 1990-2007 Dialogic Corporation. All Rights Reserved.
# The source code contained or described herein and all documents related
# to the source code ("Material") are owned by Dialogic Corporation or its
# suppliers or licensors. Title to the Material remains with Dialogic Corporation
# or its suppliers and licensors. The Material contains trade secrets and
# proprietary and confidential information of Dialogic or its suppliers and
# licensors. The Material is protected by worldwide copyright and trade secret
# laws and treaty provisions. No part of the Material may be used, copied,
# reproduced, modified, published, uploaded, posted, transmitted, distributed,
# or disclosed in any way without Dialogic's prior express written permission.
#
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be
# express and approved by Dialogic in writing.
#
###################################@@@SOFT@@@WARE@@@COPY@@@RIGHT@@@##########
#******************************************************************
#             R E V I S I O N    H I S T O R Y
#
# date______  initials  comments__________________________________
#
# 06-16-1999  PH        Modified to fix PTR14297, unable to make
#                       and make clean goes into infinite loop.
#
#******************************************************************

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

#
# Dialogic Application Tool Kit
#


#
# Updated for PTR# 14297 - SD
#

SCTOOLS_DIR= ./
SCTOOLS= $(SCTOOLS_DIR)sctools

SCTOOLS_SRC= ${INTEL_DIALOGIC_DIR}/sctools/sctools.c

#
# End of PTR# 14297 - SD
#
# Lint Files
#

#
# Demonstration Programs
#



ANSR_DIR= ./
CBANSR= $(ANSR_DIR)cbansr
CBANSR_ISDN= $(ANSR_DIR)cbansr_isdn
#DISPLAY= $(ANSR_DIR)display
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
