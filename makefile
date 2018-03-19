NAME = raytrace

SRC =	vec.c \
		obj_import.c \
		main.c \
		mlx_stuff.c \
		ply_import.c \
		scene.c \
		new_gpu_launch.c \
		true_sbvh.c \
		bvh_lab.c \
		interactive.c \
		key_command.c \
		import.c \
		strtrim.c \
    itoa.c \
    read.c \
		qdbmp/qdbmp.c \
		camera.c \
    composite.c \
    bvh_util.c \
    stl_import.c

OBJ = $(SRC:.c=.o)


FLAGS = -O3 -m64 -march=native -funroll-loops -flto
MACLIBS = mac-mlx/libmlx.a -framework OpenCL -framework OpenGL -framework AppKit
LINUXLIBS = -fopenmp linux-mlx/libmlx.a -lOpenCL -lm -lXext -lX11 


OS := $(shell uname)
ifeq ($(OS), Darwin)
	LIBS = $(MACLIBS)
else
	LIBS = $(LINUXLIBS)
endif


all: $(NAME)

$(NAME): $(OBJ)
	gcc -o $(NAME) $(FLAGS) $(OBJ) $(LIBS)
%.o: %.c rt.h
	gcc $(FLAGS) -c -o $@ $<
mac-mlx/libmlx.a:
	make -C mac-mlx
linux-mlx/libmlx.a:
	make -C linux-mlx
clean:
	rm -f $(OBJ)
fclean: clean
	rm -f $(NAME)
re:	fclean all
