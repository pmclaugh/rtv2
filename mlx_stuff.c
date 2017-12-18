#include "rt.h"

typedef struct s_data_addr
{
	int bpp;
	int size_line;
	int endian;
	char *imgbuff;
}				t_data_addr;

int color_to_int(cl_double3 c)
{
	c.x *= 255.0;
	c.y *= 255.0;
	c.z *= 255.0;
	int R = fmax(0.0, fmin(c.x, 255.0));
	int G = fmax(0.0, fmin(c.y, 255.0));
	int B = fmax(0.0, fmin(c.z, 255.0));
	int color;
	color = 0;
	color += R << 16;
	color += G << 8;
	color += B;
	return color;
}

void	draw_img_point(void *img, int x, int y, cl_double3 color)
{
	static t_data_addr *addr;

	if (!addr)
	{
		addr = (t_data_addr *)malloc(sizeof(t_data_addr));
		addr->imgbuff = mlx_get_data_addr(img, &addr->bpp, &addr->size_line, &addr->endian);
		addr->bpp /= 8;
	}
	int		i;
	int 	c;

	i = -1;
	c = color_to_int(color);
	while (++i < 4)
		addr->imgbuff[y * addr->size_line + (x * addr->bpp + i)] = ((unsigned char *)&c)[i];
}

void draw_pixels(void *img, int xres, int yres, cl_double3 *pixels)
{
	for (int y = 0; y < yres; y++)
		for (int x = 0; x < xres; x++)
			draw_img_point(img, xres - x - 1, y, pixels[y * xres + x]);
}