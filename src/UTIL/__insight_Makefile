
CC=emcc
AR=emar

SRCDIR=src
OBJDIR=obj
C_SOURCES=$(wildcard $(SRCDIR)/AST/*.c) $(wildcard $(SRCDIR)/AST/EXPR/*.c) $(wildcard $(SRCDIR)/AST/TYPE/*.c) $(wildcard $(SRCDIR)/AST/UTIL/*.c) $(wildcard $(SRCDIR)/BRIDGE/*.c) $(wildcard $(SRCDIR)/DRVR/*.c) $(wildcard $(SRCDIR)/LEX/*.c) $(wildcard $(SRCDIR)/PARSE/*.c) $(wildcard $(SRCDIR)/TOKEN/*.c) $(wildcard $(SRCDIR)/UTIL/*.c)
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
	@if not exist obj\AST\EXPR mkdir obj\AST\EXPR
	@if not exist obj\AST\TYPE mkdir obj\AST\TYPE
	@if not exist obj\AST\UTIL mkdir obj\AST\UTIL
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
	@mkdir -p obj/AST/EXPR
	@mkdir -p obj/AST/TYPE
	@mkdir -p obj/AST/UTIL
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
	del obj\AST\EXPR\*.* /Q
	del obj\AST\TYPE\*.* /Q
	del obj\AST\UTIL\*.* /Q
	del obj\BRIDGE\*.* /Q
	del obj\DRVR\*.* /Q
	del obj\LEX\*.* /Q
	del obj\PARSE\*.* /Q
	del obj\TOKEN\*.* /Q
	del obj\UTIL\*.* /Q
else
	rm -f 2> /dev/null $(INSIGHT)
	rm -f 2> /dev/null obj/AST/*.*
	rm -f 2> /dev/null obj/AST/EXPR/*.*
	rm -f 2> /dev/null obj/AST/TYPE/*.*
	rm -f 2> /dev/null obj/AST/UTIL/*.*
	rm -f 2> /dev/null obj/BRIDGE/*.*
	rm -f 2> /dev/null obj/DRVR/*.*
	rm -f 2> /dev/null obj/LEX/*.*
	rm -f 2> /dev/null obj/PARSE/*.*
	rm -f 2> /dev/null obj/TOKEN/*.*
	rm -f 2> /dev/null obj/UTIL/*.*
endif
