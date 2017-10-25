#include "rt.h"

//completely arbitrary values
#define ka 0.5
#define kd 2
#define ks 1

#define Ia 1

#define FUZZ rand_unit() / 700.0

#define H_FOV M_PI_2 * 60.0 / 90.0 	//60 degrees. eventually this will be dynamic with aspect
									// V_FOV is implicit with aspect of view-plane

t_ray ray_from_camera(t_camera cam, float x, float y)
{
	t_ray r;
	r.origin = cam.focus;
	t_float3 through = cam.origin;
	through = vec_add(through, vec_scale(cam.d_x, (float)x));
	through = vec_add(through, vec_scale(cam.d_y, (float)y));
	r.direction = unit_vec(vec_sub(cam.focus, through));
	r.inv_dir = vec_inv(r.direction);
	return r;
}

void init_camera(t_camera *camera, int xres, int yres)
{
	printf("init camera %d %d\n", xres, yres);
	//determine a focus point in front of the view-plane
	//such that the edge-focus-edge vertex has angle H_FOV
	float d = (camera->width / 2.0) / tan(H_FOV / 2.0);
	camera->focus = vec_add(camera->center, vec_scale(camera->normal, d));

	//now i need unit vectors on the plane
	t_3x3 camera_matrix = rotation_matrix(UNIT_Z, camera->normal);
	t_float3 camera_x, camera_y;
	camera_x = mat_vec_mult(camera_matrix, UNIT_X);
	camera_y = mat_vec_mult(camera_matrix, UNIT_Y);

	//pixel deltas
	camera->d_x = vec_scale(camera_x, camera->width / (float)xres);
	camera->d_y = vec_scale(camera_y, camera->height / (float)yres);

	//start at bottom corner (the plane's "origin")
	camera->origin = vec_sub(camera->center, vec_scale(camera_x, camera->width / 2.0));
	camera->origin = vec_sub(camera->origin, vec_scale(camera_y, camera->height / 2.0));
}

#define DIFFUSE 0.6

void trace(t_ray *ray, t_float3 *color, const t_scene *scene, int depth)
{
	float rrFactor = 1.0;
	if(depth > 20)
		return ; //this is mathematically bad, but good while testing
	if (depth >= 5)
	{
		float stop_prob = 0.1;
		if (rand_unit() <= stop_prob)
			return;
		rrFactor = 1.0 / (1.0 - stop_prob);
	}

	float closest_dist = FLT_MAX;
	t_object *closest_object = NULL;
	hit_nearest(ray, scene->bvh, &closest_object, &closest_dist);

	if (closest_object == NULL)
		return;

	t_float3 intersection = vec_add(ray->origin, vec_scale(ray->direction, closest_dist));
	ray->origin = intersection;
	t_float3 N = norm_object(closest_object, ray);
	

	float emission = closest_object->emission;
	*color = vec_add(*color, (t_float3){emission * rrFactor, emission * rrFactor, emission * rrFactor});

	//diffuse reflection
	t_float3 hem_x, hem_y;
	orthonormal(N, &hem_x, &hem_y);
	t_float3 rand_dir = better_hemisphere(rand_unit(), rand_unit());
	t_float3 rotated_dir;
	rotated_dir.x = dot(rand_dir, (t_float3){hem_x.x, hem_y.x, N.x});
	rotated_dir.y = dot(rand_dir, (t_float3){hem_x.y, hem_y.y, N.y});
	rotated_dir.z = dot(rand_dir, (t_float3){hem_x.z, hem_y.z, N.z});
	ray->direction = unit_vec(rotated_dir);
	ray->inv_dir = vec_inv(ray->direction);
	float cost = dot(ray->direction, N);

	t_float3 tmp = BLACK;
	trace(ray, &tmp, scene, depth + 1);
	*color = vec_add(*color, vec_scale(vec_had(tmp, closest_object->color), cost * DIFFUSE * rrFactor));
}

t_float3 *simple_render(const t_scene *scene, const int xres, const int yres)
{
	clock_t start, end;
	start = clock();
	int spp = 20;
	t_float3 *pixels = calloc(xres * yres, sizeof(t_float3));
	
	for (int y = 0; y < yres; y++)
	{
		for (int x = 0; x < xres; x++)
		{
			t_halton h1, h2;
			h1 = setup_halton(0,2);
			h2 = setup_halton(0,3);
			for (int s = 0; s < spp; s++)
			{
				t_ray r = ray_from_camera(scene->camera, (float)x + FUZZ, (float)y + FUZZ);
				t_float3 c = BLACK;
				trace(&r, &c, scene, 0);
				pixels[y * xres + x] = vec_add(pixels[y * xres + x], vec_scale(c, 1.0 / (float)spp));
			}
		}
		if (y % 10 == 0)
			printf("%.2f%% done\n", 100.0 * (float)y / (float)yres);
	}
	float lmax = 0.0;
	float lsum = 0.0;
	for (int i =0 ; i < yres * xres; i++)
	{
		float l = max3(pixels[i].x, pixels[i].y, pixels[i].z);
		lsum += l;
		if (l > lmax)
			lmax = l;
	}

	printf("highest luminance was %f\n", lmax);
	printf("avg luminance was %f\n", lsum / (float)(yres * xres));

	end = clock();
	double cpu_time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("trace time %lf sec, %lf FPS\n", cpu_time, 1.0 / cpu_time);
	return pixels;
}