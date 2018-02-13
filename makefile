NAME = raytrace

SRCS = vec.c obj_import.c main.c mlx_stuff.c ply_import.c scene.c new_gpu_launch.c true_sbvh.c
OBJS = vec.o obj_import.o main.o mlx_stuff.o ply_import.o scene.o new_gpu_launch.o true_sbvh.o


FLAGS = -g
MACLIBS = mac-mlx/libmlx.a -framework OpenCL -framework OpenGL -framework AppKit
LINUXLIBS = linux-mlx/libmlx.a -lOpenCL -lm -lXext -lX11 -fopenmp


OS := $(shell uname)
ifeq ($(OS), Darwin)
	LIBS = $(MACLIBS)
else
	LIBS = $(LINUXLIBS)
endif


all: $(NAME)

$(NAME): $(OBJS)
	gcc -o $(NAME) $(FLAGS) $(OBJS) $(LIBS)
%.o: %.c rt.h
	gcc $(FLAGS) -c -o $@ $< -lm
mac-mlx/libmlx.a:
	make -C mac-mlx
linux-mlx/libmlx.a:
	make -C linux-mlx
clean:
	rm -f $(OBJS)
fclean: clean
	rm -f $(NAME)
re:	fclean all
