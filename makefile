NAME = raytrace

SRCS = mlx_stuff.c object.c render.c vec.c obj_import.c bvh.c main.c monte.c
OBJS = mlx_stuff.o object.o render.o vec.o obj_import.o bvh.o main.o monte.o mlx/libmlx.a

FLAGS = -m64 -O3 -flto -march=native -funroll-loops

all: $(NAME)

$(NAME): $(OBJS)
	gcc-7 -o $(NAME) $(FLAGS) $(OBJS) -framework OpenGL -framework AppKit -fopenmp
%.o: %.c rt.h
	gcc-7 $(FLAGS) -c -o $@ $< -fopenmp
mlx/libmlx.a:
	make -C mlx
clean:
	rm -f $(OBJS)
fclean: clean
	rm -f $(NAME)
re:	fclean all
