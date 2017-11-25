NAME = raytrace


SRCS = vec.c obj_import.c bvh.c main.c gpu_launch.c mlx_stuff.c
OBJS = vec.o obj_import.o bvh.o main.o gpu_launch.o mlx_stuff.o mlx/libmlx_Linux.a


FLAGS = -m64 -O3 -flto -march=native -funroll-loops

all: $(NAME)

$(NAME): $(OBJS)
	gcc -o $(NAME) $(FLAGS) $(OBJS) -lOpenCL -lm -lXext -lX11
%.o: %.c rt.h
	gcc $(FLAGS) -c -o $@ $< -lOpenCL -lm
clean:
	rm -f $(OBJS)
fclean: clean
	rm -f $(NAME)
re:	fclean all
