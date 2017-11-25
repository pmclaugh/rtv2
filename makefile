NAME = raytrace


SRCS = object.c render.c vec.c obj_import.c bvh.c main.c monte.c gpu_launch.c
OBJS = object.o render.o vec.o obj_import.o bvh.o main.o monte.o gpu_launch.o


FLAGS = -m64 -O3 -flto -march=native -funroll-loops

all: $(NAME)

$(NAME): $(OBJS)
	gcc -o $(NAME) $(FLAGS) $(OBJS) -lOpenCL
%.o: %.c rt.h
	gcc $(FLAGS) -c -o $@ $< -lOpenCL
clean:
	rm -f $(OBJS)
fclean: clean
	rm -f $(NAME)
re:	fclean all
