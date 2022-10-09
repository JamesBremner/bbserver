#source file search paths
VPATH = ./src 

	# compiler include search paths
INCS = -I./src/ravencode \


LIBS = \
	-lws2_32 -lwsock32 \
	-lgdiplus -lgdi32 -lcomdlg32 -lstdc++fs \

	#-mwindows

static_link_flags = -static-libstdc++ -static-libgcc -static

# folders
ODIR = ./.vscode/obj
RDIR = ./src/ravencode

OPT = -g
CCflags = -std=c++17 $(OPT) -D_mingw_

# sources
OBJ_bbserver = \
	$(ODIR)/main.o \
	$(ODIR)/ctcp.o \
	$(ODIR)/cTCPServerMultiClient.o

OBJ_test = \
	$(ODIR)/test.o \
	$(ODIR)/ctcp.o	

### COMPILE

$(ODIR)/ctcp.o: $(RDIR)/ctcp.cpp $(RDIR)/ctcp.h
	gcc -c -o $@  \
	$< $(CCflags)
$(ODIR)/cTCPServerMultiClient.o: $(RDIR)/cTCPServerMultiClient.cpp $(RDIR)/ctcp.h
	gcc -c -o $@  \
	$< $(CCflags)

$(ODIR)/%.o: %.cpp $(RDIR)/tcp.h  
	g++  $(CCflags) \
	-c -o $@ $< $(INCS)

### LINK

bbserver: $(OBJ_bbserver)
	g++ $(OPT) \
	-o bin/starter.exe $^  \
	$(static_link_flags) $(LIBS)

test: $(OBJ_test)
	g++ $(OPT) \
	-o bin/test.exe $^  \
	$(static_link_flags) $(LIBS)

.PHONY: clean
clean:
	del /q .vscode\\obj\\*.o

