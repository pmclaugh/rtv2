
__constant float PI = 3.14159265359f;
__constant int SAMPLES = 2000;
__constant int MAXDEPTH = 30;

enum type {SPHERE, TRIANGLE};
enum mat {MAT_DIFFUSE, MAT_SPECULAR, MAT_REFRACTIVE, MAT_NULL};

typedef struct s_ray {
	float3 direction;
	float3 origin;
}				Ray;

typedef struct s_object
{
	enum type shape;
	enum mat material;
	float3 v0;
	float3 v1;
	float3 v2;
	float3 color;
	float3 emission;
}				Object;

typedef struct s_halton
{
	double value;
	double inv_base;
}				Halton;

typedef struct s_camera
{
	float3 origin;
	float3 focus;
	float3 d_x;
	float3 d_y;
}				Camera;

Halton setup_halton(int i, int base)
{
	Halton hal;
	double f = 1.0 / (double)base;
	hal.inv_base = f;
	hal.value = 0.0;
	while(i > 0)
	{
		hal.value += f * (double)(i % base);
		i /= base;
		f *= hal.inv_base;
	}
	return hal;
}

double next_hal(Halton *hal)
{
	double r = 1.0 - hal->value - 0.0000001;
	if (hal->inv_base < r)
		hal->value += hal->inv_base;
	else
	{
		double h = hal->inv_base;
		double hh;
		do {hh = h; h *= hal->inv_base;} while(h >= r);
		hal->value += hh + h - 1.0;
	}
	return hal->value;
}

int intersect_sphere(Ray ray, Object obj, float *t)
{
	float3 o_min_c = ray.origin - obj.v1;

	float a = dot(ray.direction, o_min_c);
	float b = length(o_min_c);
	float c = obj.v2.x; //this is the radius

	float radicand = a * a - b * b + c * c; //probably a better way to do this

	if (radicand < 0)
		return 0;
	float d = -1 * a;
	if (radicand > 0)
	{
		float dplus, dminus;
		radicand = sqrt(radicand);
		dplus = d + radicand;
		dminus = d - radicand;

		if (dminus > 0)
			d = dplus;
		else if (dplus > 0)
			d = dplus;
		else
			return 0;
	}
	else
		return (0);
	*t = d;
	return (1);
}

int intersect_triangle(Ray ray, Object obj, float *t)
{
	float3 e1 = obj.v1 - obj.v0;
	float3 e2 = obj.v2 - obj.v0;

	float3 h = cross(ray.direction, e2);
	float a = dot(h, e1);

	if (a < 0)
		return 0;
	float f = 1.0f / a;
	float3 s = ray.origin - obj.v0;
	float u = f * dot(s, h);
	if (u < 0.0 || u > 1.0)
		return 0;
	float3 q = cross(s, e1);
	float v = f * dot(ray.direction, q);
	if (v < 0.0 || u + v > 1.0)
		return 0;
	float this_t = f * dot(e2, q);
	if (this_t > 0)
	{
		*t = this_t;
		return (1);
	}
	else
		return (0);
}

int hit_nearest_brute(Ray ray, __constant Object *scene, int object_count, float *t)
{
	float t_min = FLT_MAX;
	int best_ind = -1;
	for (int i = 0; i < object_count; i++)
	{
		float this_t;
		if (scene[i].shape == SPHERE)
		{
			if (intersect_sphere(ray, scene[i], &this_t) && this_t < t_min)
			{
				t_min = this_t;
				best_ind = i;
			}
		}
		else
		{
			if (intersect_triangle(ray, scene[i], &this_t) && this_t < t_min)
			{
				t_min = this_t;
				best_ind = i;
			}
		}
	}
	return best_ind;
}

float3 trace(Ray ray, __constant Object *scene, int object_count, Halton *h1, Halton *h2)
{
	float3 color = (float3)(0.0f, 0.0f, 0.0f);
	float3 mask = (float3)(1.0f, 1.0f, 1.0f);
	for (int j = 0; j < MAXDEPTH; j++)
	{
		//collide
		float t;
		int hit_ind = hit_nearest_brute(ray, scene, object_count, &t);
		if (hit_ind == -1)
			break ;
		Object hit = scene[hit_ind];
		float3 hit_point = ray.origin + ray.direction * t;

		//get normal at collision point
		float3 N;
		if (hit.shape == SPHERE)
			N = normalize(hit_point - hit.v1);
		else
			N = normalize(cross(hit.v1 - hit.v0, hit.v2 - hit.v0));
		N = dot(ray.direction, N) < 0.0f ? N : -1.0f * N;

		//local orthonormal system
		float3 axis = fabs(N.x) > fabs(N.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
		float3 hem_x = normalize(cross(axis, N)); // I don't think normalize is necessary here (?)
		float3 hem_y = cross(N, hem_x);

		//generate random direction on the unit hemisphere
		float rsq = next_hal(h1);
		float r = sqrt(rsq);
		float theta = 2 * PI * next_hal(h2);

		//combine for new direction
		float3 rand_dir = normalize(hem_x * r * cos(theta) + hem_y * r * sin(theta) + N * sqrt(max(0.0f, 1.0f - rsq))); //again is normalize necessary?

		//consolidate
		ray.origin = hit_point + N * 0.000001f;
		ray.direction = rand_dir;
		color += mask * closest_object.emission;
		mask *= closest_object.color;
		mask *= dot(rand_dir, N);
	}
	return color;
}

Ray ray_from_cam(Camera cam, float x, float y)
{
	Ray ray;
	ray.origin = cam.focus;
	float3 through = cam.origin + cam.d_x * x + cam.d_y *y;
	ray.direction = normalize(through - origin);
	return ray;
}

__kernel void render_kernel(__constant Object *scene, __constant Camera cam, const int width, const int height, const int obj_count, __global float3* output)
{
	unsigned int work_item_id = get_global_id(0);
	unsigned int x = work_item_id % width;
	unsigned int y = work_item_id / width;

	float3 sum_color = (float3)(0.0f, 0.0f, 0.0f);

	Halton h1, h2;

	h1 = setup_halton(0,2);
	h2 = setup_halton(0,3);

	float inv_samples = 1.0 / (float)SAMPLES;
	for (int i = 0; i < SAMPLES; i++)
	{
		//make the ray
		Ray ray = ray_from_cam(cam, (float)width + next_hal(&h1), (float)height + next_hal(&h2));

		//trace the ray and save result
		sum_color += trace(ray, scene, obj_count, h1, h2) * inv_samples;
	}
	output[work_item_id] = sum_color;
}