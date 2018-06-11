NAME = raytrace

SRC =	vec.c \
		obj_import.c \
		main.c \
		mlx_stuff.c \
		ply_import.c \
		new_gpu_launch.c \
		bvh.c \
		bvh_lab.c \
		spatial.c \
		interactive.c \
		key_command.c \
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
		libjpeg/jerror.c \
		libjpeg/jdapimin.c \
		libjpeg/jdatasrc.c \
		libjpeg/jdapistd.c \
		libjpeg/jdmaster.c \
		libjpeg/jmemmgr.c \
		libjpeg/jdmarker.c \
		libjpeg/jdinput.c \
		libjpeg/jcomapi.c \
		libjpeg/jutils.c \
		libjpeg/jmemansi.c \
		libjpeg/jquant1.c \
		libjpeg/jquant2.c \
		libjpeg/jdcolor.c \
		libjpeg/jdsample.c \
		libjpeg/jddctmgr.c \
		libjpeg/jidctint.c \
		libjpeg/jidctfst.c \
		libjpeg/jidctflt.c \
		libjpeg/jdmerge.c \
		libjpeg/jdmainct.c \
		libjpeg/jdpostct.c \
		libjpeg/jdarith.c \
		libjpeg/jdhuff.c \
		libjpeg/jdcoefct.c \
		libjpeg/jaricom.c


OBJ = $(SRC:.c=.o)


INC = libjpeg
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
	gcc -o $(NAME) $(FLAGS) -I $(INC) $(OBJ) $(LIBS)
%.o: %.c rt.h
	gcc $(FLAGS) -I $(INC) -c -o $@ $<
libjpeg/%.o: %.c rt.h
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
