CFLAGS = $(shell sdl-config --cflags) -Wall
CXXFLAGS = $(CFLAGS)
LIBS = -lSDL_image -lSDL_ttf $(shell sdl-config --libs)


test: prueba.o 
	gcc $(CFLAGS) -o prueba prueba.c $(LIBS)

clean:
	rm -f \
		$(TARGETDIR_prueba.c)/prueba.c \
		$(TARGETDIR_prueba.c)/prueba.o
	rm -f -r $(TARGETDIR_prueba.c)


