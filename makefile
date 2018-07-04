NAME = clive

SRC =	main.c \
		sdl.c \
		input.c \
		vec.c \
		import.c \
		obj_import.c \
		ply_import.c \
		new_gpu_launch.c \
		bvh.c \
		bvh_lab.c \
		spatial.c \
		ia_mode.c \
		mv_mode.c \
		str.c \
		itoa.c \
		read.c \
		qdbmp/qdbmp.c \
		camera.c \
		composite.c \
		bvh_util.c \
		stl_import.c \
		get_face.c

OBJ = $(SRC:.c=.o)


FLAGS = -O3 -m64 -march=native -funroll-loops -flto
MACLIBS = -framework OpenCL -framework OpenGL -framework AppKit
LINUXLIBS = -fopenmp -lOpenCL -lm -lXext -lX11 


OS := $(shell uname)
ifeq ($(OS), Darwin)
	LIBS = $(MACLIBS)
else
	LIBS = $(LINUXLIBS)
endif


all: $(NAME)

install:
	cd SDL2-2.0.8; ./configure; make; sudo make install

$(NAME): $(OBJ)
	gcc -o $(NAME) $(FLAGS) $(OBJ) $(LIBS) -I -L -lSDL2
%.o: %.c rt.h
	gcc $(FLAGS) -c -o $@ $<
libjpeg/%.o: %.c rt.h
	gcc $(FLAGS) -c -o $@ $<
clean:
	rm -f $(OBJ)
fclean: clean
	rm -f $(NAME)
re:	fclean all
