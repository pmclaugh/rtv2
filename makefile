NAME = clive

SRC =	vec.c \
		obj_import.c \
		main.c \
		ply_import.c \
		new_gpu_launch.c \
		bvh.c \
		bvh_lab.c \
		interactive.c \
		input.c \
		import.c \
		str.c \
		itoa.c \
		read.c \
		qdbmp/qdbmp.c \
		camera.c \
		composite.c \
		bvh_util.c \
		stl_import.c \
		get_face.c \
		sdl.c

OBJ = $(SRC:.c=.o)

INC = libjpeg
FLAGS = -O3 -m64 -march=native -funroll-loops -flto
MACLIBS = mac-mlx/libmlx.a -framework OpenCL -framework OpenGL -framework AppKit
LINUXLIBS = -fopenmp linux-mlx/libmlx.a -lOpenCL -lm -lXext -lX11

bundle_contents = CLIVE.app/Contents

OS := $(shell uname)
ifeq ($(OS), Darwin)
	LIBS = $(MACLIBS)
else
	LIBS = $(LINUXLIBS)
endif


all: $(NAME)

app: CLIVE_bundle

CLIVE_bundle: $(NAME)
	mkdir -p $(bundle_contents)/MacOS
	mkdir -p $(bundle_contents)/Resources
	echo "APPL????" > $(bundle_contents)/PkgInfo
	$(INSTALL_PROGRAM) $< $(bundle_contents)/MacOS/

# gcc main.c -I SDL2-2.0.8/include -lSDL2.a

$(NAME): $(OBJ)
	gcc -o $(NAME) $(FLAGS) -I $(INC) $(OBJ) $(LIBS) -L libjpeg -ljpeg -lSDL2
%.o: %.c rt.h
	gcc $(FLAGS) -I $(INC) -c -o $@ $<
mac-mlx/libmlx.a:
	make -C mac-mlx
linux-mlx/libmlx.a:
	make -C linux-mlx
clean:
	rm -f $(OBJ)
fclean: clean
	rm -f $(NAME)
re:	fclean all
