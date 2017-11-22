#include "rt.h"

typedef struct s_param
{
	void *mlx;
	void *win;
	void *img;
	int x;
	int y;
}				t_param;

#define rot_angle M_PI_2 * 4.0 / 90.0

void baby_tone_map(cl_float3 *pixels, int count)
{
	for (int i = 0; i < count; i++)
	{
		pixels[i].x = pixels[i].x / (pixels[i].x + 1.0);
		pixels[i].y = pixels[i].y / (pixels[i].y + 1.0);
		pixels[i].z = pixels[i].z / (pixels[i].z + 1.0);
	}
}

// int loop_hook(void *param)
// {
// 	t_param *p = (t_param *)param;

// 	cl_float3 *pixels = gpu_render(p->scene, p->scene->camera);
// 	baby_tone_map(pixels, xdim * ydim);
// 	draw_pixels(p->img, p->x, p->y, pixels);
// 	mlx_put_image_to_window(p->mlx, p->win, p->img, 0, 0);
// 	free(pixels);
// 	p->scene->camera.center = vec_add(p->scene->camera.center, (cl_float3){0.0, 0.0, 0.1});
// 	init_camera(&p->scene->camera, xdim, ydim);
// 	return (1);
// }

int key_hook(int keycode, void *param)
{
	t_param *p = (t_param *)param;

	if (keycode == 12)
	{
		mlx_destroy_image(p->mlx, p->img);
		mlx_destroy_window(p->mlx, p->win);
		exit(0);
	}
	return (1);
}

#define ROOMSIZE 10.0

float vmag(cl_float3 v)
{
	return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float vsum(cl_float3 v)
{
	return v.x + v.y + v.z;
}

float vmax(cl_float3 v)
{
	return fmax(fmax(v.x, v.y), v.z);
}

void reinhard_tone_map(cl_float3 *pixels, int count)
{
	//get greyscale luminance values for all pixels,
	//and pixels become color masks
	float *lums = calloc(count, sizeof(float));
	for (int i = 0; i < count; i++)
	{
		lums[i] = vmag(pixels[i]);
		pixels[i].x /= lums[i];
		pixels[i].y /= lums[i];
		pixels[i].z /= lums[i];
	}

	//compute log average of luminances
	double lavg = 0.0;
	for (int i = 0; i < count; i++)
		lavg += log(lums[i]);
	printf("lavg before exp: %lf\n", lavg / (double)count);
	lavg = exp(lavg / (double)count);

	printf("lavg: %lf\n", lavg);
	//map so log-average value is now mid-gray
	for (int i = 0; i < count; i++)
		lums[i] = lums[i] * 0.5 / lavg;

	//compress high luminances
	for (int i = 0; i < count; i++)
		lums[i] = lums[i] / (lums[i] + 1.0);

	//re-color
	for (int i = 0; i < count; i++)
	{
		pixels[i].x *= lums[i];
		pixels[i].y *= lums[i];
		pixels[i].z *= lums[i];
	}
}

int main(int ac, char **av)
{
	srand(time(NULL));

	Scene *scene = scene_from_obj("objects/sponza/", "sponza.obj");

	printf("loaded scene. it has %d faces and %d materials\n", scene->face_count, scene->mat_count);

	t_camera cam;
	cam.center = (cl_float3){500.0, 180.0, 100.0};
	cam.normal = (cl_float3){0.0, 0.0, 1.0};
	cam.normal = unit_vec(cam.normal);
	cam.width = 1.0;
	cam.height = 1.0;
	init_camera(&cam, xdim, ydim);

	cl_float3 *pixels = gpu_render(scene, cam);

	baby_tone_map(pixels, xdim * ydim);
	printf("tone mapped\n");
	
	void *mlx = mlx_init();
	void *win = mlx_new_window(mlx, xdim, ydim, "RTV1");
	void *img = mlx_new_image(mlx, xdim, ydim);
	draw_pixels(img, xdim, ydim, pixels);
	mlx_put_image_to_window(mlx, win, img, 0, 0);

	t_param *param = calloc(1, sizeof(t_param));
	*param = (t_param){mlx, win, img, xdim, ydim};
	
	mlx_key_hook(win, key_hook, param);
	mlx_loop(mlx);

}