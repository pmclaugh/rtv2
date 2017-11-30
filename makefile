NAME = raytrace

SRCS = vec.c obj_import.c main.c gpu_launch.c mlx_stuff.c sbvh.c ply_import.c scene.c
OBJS = vec.o obj_import.o main.o gpu_launch.o mlx_stuff.o sbvh.o ply_import.o scene.o


FLAGS = -m64 -O3 -flto -march=native -funroll-loops
MACLIBS = mac-mlx/libmlx.a -framework OpenCL -framework OpenGL -framework AppKit
LINUXLIBS = linux-mlx/libmlx.a -lOpenCL -lm -lXext -lX11


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
