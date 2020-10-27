
ifeq ($(OS), Windows_NT)
	CC=x86_64-w64-mingw32-gcc
	AR=x86_64-w64-mingw32-gcc-ar
else
	CC=gcc
	AR=ar
endif

SRCDIR=src
OBJDIR=obj
C_SOURCES=$(wildcard $(SRCDIR)/AST/*.c) $(wildcard $(SRCDIR)/BRIDGE/*.c) $(wildcard $(SRCDIR)/DRVR/*.c) $(wildcard $(SRCDIR)/LEX/*.c) $(wildcard $(SRCDIR)/PARSE/*.c) $(wildcard $(SRCDIR)/TOKEN/*.c) $(wildcard $(SRCDIR)/UTIL/*.c)
C_OBJECTS=$(C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
INSIGHT=../../obj/insight.a
CFLAGS=-c -Wall -I"include" -O3 -DADEPT_INSIGHT_BUILD # -fmax-errors=5 -Werror

release: directories $(INSIGHT)

$(INSIGHT): $(C_OBJECTS)
	$(AR) -rcs $(INSIGHT) $(C_OBJECTS)

$(C_OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $< -o $@

ifeq ($(OS), Windows_NT)
directories:
	@if not exist obj mkdir obj
	@if not exist obj\AST mkdir obj\AST
	@if not exist obj\BRIDGE mkdir obj\BRIDGE
	@if not exist obj\DRVR mkdir obj\DRVR
	@if not exist obj\LEX mkdir obj\LEX
	@if not exist obj\PARSE mkdir obj\PARSE
	@if not exist obj\TOKEN mkdir obj\TOKEN
	@if not exist obj\UTIL mkdir obj\UTIL
else
directories:
	@mkdir -p obj
	@mkdir -p obj/AST
	@mkdir -p obj/BRIDGE
	@mkdir -p obj/DRVR
	@mkdir -p obj/LEX
	@mkdir -p obj/PARSE
	@mkdir -p obj/TOKEN
	@mkdir -p obj/UTIL
endif

clean:
ifeq ($(OS), Windows_NT)
	if exist $(INSIGHT) del $(INSIGHT) /Q
	del obj\AST\*.* /Q
	del obj\BRIDGE\*.* /Q
	del obj\DRVR\*.* /Q
	del obj\LEX\*.* /Q
	del obj\PARSE\*.* /Q
	del obj\TOKEN\*.* /Q
	del obj\UTIL\*.* /Q
else
	rm -f 2> /dev/null $(INSIGHT)
	rm -f 2> /dev/null obj/AST/*.*
	rm -f 2> /dev/null obj/BRIDGE/*.*
	rm -f 2> /dev/null obj/DRVR/*.*
	rm -f 2> /dev/null obj/LEX/*.*
	rm -f 2> /dev/null obj/PARSE/*.*
	rm -f 2> /dev/null obj/TOKEN/*.*
	rm -f 2> /dev/null obj/UTIL/*.*
endif
