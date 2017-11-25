#include "rt.h"

typedef struct s_param
{
	void *mlx;
	void *win;
	void *img;
	int x;
	int y;

	Scene *scene;
	t_camera cam;
	cl_float3 *pixels;
	int samplecount;
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

cl_float3 *baby_tone_dupe(cl_float3 *pixels, int count, int samplecount)
{
	cl_float3 *toned = calloc(count, sizeof(cl_float3));
	for (int i = 0; i < count; i++)
	{
		pixels[i].x = pixels[i].x / (float)samplecount;
		pixels[i].y = pixels[i].y / (float)samplecount;
		pixels[i].z = pixels[i].z / (float)samplecount;
		toned[i].x = pixels[i].x / (pixels[i].x + 1.0);
		toned[i].y = pixels[i].y / (pixels[i].y + 1.0);
		toned[i].z = pixels[i].z / (pixels[i].z + 1.0);
	}
	return toned;
}

int loop_hook(void *param)
{
	t_param *p = (t_param *)param;

	if (!p->pixels)
		p->pixels = calloc(xdim *ydim, sizeof(cl_float3));
	cl_float3 *new = gpu_render(p->scene, p->cam);
	p->samplecount++;
	for (int i = 0; i < xdim * ydim; i++)
		p->pixels[i] = vec_add(p->pixels[i], new[i]);

	cl_float3 *draw = baby_tone_dupe(p->pixels, xdim * ydim, p->samplecount);
	draw_pixels(p->img, p->x, p->y, draw);
	mlx_put_image_to_window(p->mlx, p->win, p->img, 0, 0);
	free(draw);
	free(new);
	printf("%d samples\n", p->samplecount * 20);
	return (1);
}

#define H_FOV M_PI_2 * 60.0 / 90.0 	//60 degrees. eventually this will be dynamic with aspect
									// V_FOV is implicit with aspect of view-plane

void init_camera(t_camera *camera, int xres, int yres)
{
	//printf("init camera %d %d\n", xres, yres);
	//determine a focus point in front of the view-plane
	//such that the edge-focus-edge vertex has angle H_FOV
	float d = (camera->width / 2.0) / tan(H_FOV / 2.0);
	camera->focus = vec_add(camera->center, vec_scale(camera->normal, d));

	//now i need unit vectors on the plane
	t_3x3 camera_matrix = rotation_matrix(UNIT_Z, camera->normal);
	cl_float3 camera_x, camera_y;
	camera_x = mat_vec_mult(camera_matrix, UNIT_X);
	camera_y = mat_vec_mult(camera_matrix, UNIT_Y);

	//pixel deltas
	camera->d_x = vec_scale(camera_x, camera->width / (float)xres);
	camera->d_y = vec_scale(camera_y, camera->height / (float)yres);

	//start at bottom corner (the plane's "origin")
	camera->origin = vec_sub(camera->center, vec_scale(camera_x, camera->width / 2.0));
	camera->origin = vec_sub(camera->origin, vec_scale(camera_y, camera->height / 2.0));
}

int key_hook(int keycode, void *param)
{
	t_param *p = (t_param *)param;
	printf("%d\n", keycode);
	if (keycode == 113)
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

	//bvh construction now happens inside scene_from_obj
	Scene *scene = scene_from_obj("objects/sponza/", "sponza.obj");

	printf("loaded scene. it has %d faces and %d materials\n", scene->face_count, scene->mat_count);

	

	t_camera cam;

	//pointed right at the lion
	cam.center = (cl_float3){-200.0, 50.0, 100.0};
	cam.normal = (cl_float3){-1.0, 0.0, 0.0};

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
	*param = (t_param){mlx, win, img, xdim, ydim, scene, cam, NULL, 0};
	
	mlx_key_hook(win, key_hook, param);
	mlx_loop_hook(mlx, loop_hook, param);
	mlx_loop(mlx);
}