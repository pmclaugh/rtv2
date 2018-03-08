NAME = raytrace

SRCS = vec.c obj_import.c main.c mlx_stuff.c ply_import.c scene.c \
new_gpu_launch.c true_sbvh.c bvh_lab.c import.c strtrim.c qdbmp/qdbmp.c
OBJS = vec.o obj_import.o main.o mlx_stuff.o ply_import.o scene.o \
new_gpu_launch.o true_sbvh.o bvh_lab.o import.o strtrim.o qdbmp/qdbmp.o


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

$(NAME): $(OBJS)
	gcc -o $(NAME) $(FLAGS) $(OBJS) $(LIBS)
%.o: %.c rt.h
	gcc $(FLAGS) -c -o $@ $< -lm -w
mac-mlx/libmlx.a:
	make -C mac-mlx
linux-mlx/libmlx.a:
	make -C linux-mlx
clean:
	rm -f $(OBJS)
fclean: clean
	rm -f $(NAME)
re:	fclean all
