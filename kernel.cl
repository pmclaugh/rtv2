
__constant float PI = 3.14159265359f;
__constant float REFRACTIVE_INDEX = 1.5;
__constant int SAMPLES_PER_LAUNCH = 1024;
__constant int POOLSIZE = 256;
__constant float COLLIDE_ERR = 0.00001f;
__constant float NORMAL_SHIFT = 0.00003f;

#define SPHERE 1
#define TRIANGLE 2

#define MAT_DIFFUSE 1
#define MAT_SPECULAR 2
#define MAT_REFRACTIVE 3
#define MAT_NULL 4

#define BLACK (float3)(0.0f, 0.0f, 0.0f)
#define WHITE (float3)(1.0f, 1.0f, 1.0f)
#define GREY (float3)(0.5, 0.5, 0.5)

typedef struct s_ray {
	float3 origin;
	float3 direction;
}				Ray;

typedef struct s_object
{
	int shape;
	float3 mat_probabilities;
	float3 mat_constants;
	float3 v0;
	float3 v1;
	float3 v2;
	float3 color;
	float3 emission;
}				Object;

typedef struct s_camera
{
	float3 origin;
	float3 focus;
	float3 d_x;
	float3 d_y;
}				Camera;

static float get_random(unsigned int *seed0, unsigned int *seed1) {

	/* hash the seeds using bitwise AND operations and bitshifts */
	*seed0 = 36969 * ((*seed0) & 65535) + ((*seed0) >> 16);  
	*seed1 = 18000 * ((*seed1) & 65535) + ((*seed1) >> 16);

	unsigned int ires = ((*seed0) << 16) + (*seed1);

	/* use union struct to convert int to float */
	union {
		float f;
		unsigned int ui;
	} res;

	res.ui = (ires & 0x007fffff) | 0x40000000;  /* bitwise AND, bitwise OR */
	return (res.f - 2.0f) / 2.0f;
}

int intersect_sphere(Ray ray, Object obj, float *t)
{
	float3 toCenter = obj.v0 - ray.origin;
	float b = dot(toCenter, ray.direction);
	float c = dot(toCenter, toCenter) - obj.v1.x * obj.v1.x;
	float disc = b * b - c;

	if (disc < 0.0f) return 0;
	else disc = sqrt(disc);

	if ((b - disc) > COLLIDE_ERR)
	{
		*t = b - disc;
		return 1;
	}
	if ((b + disc) > COLLIDE_ERR) 
	{
		*t = b + disc;
		return 1;
	}

	return 0;
}

int intersect_triangle(Ray ray, Object obj, float *t)
{
	float3 e1 = obj.v1 - obj.v0;
	float3 e2 = obj.v2 - obj.v0;

	float3 h = cross(ray.direction, e2);
	float a = dot(h, e1);

	if (fabs(a) < 0)
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
	if (this_t > COLLIDE_ERR)
	{
		*t = this_t;
		return (1);
	}
	else
		return (0);
}

int hit_nearest_brute(const Ray ray, __constant Object *scene, int object_count, float *t)
{
	float t_min = FLT_MAX;
	int best_ind = -1;
	for (int i = 0; i < object_count; i++)
	{
		float this_t = FLT_MAX;
		Object obj = scene[i];
		if (obj.shape == SPHERE)
		{
			if (intersect_sphere(ray, obj, &this_t) && this_t < t_min)
			{
				t_min = this_t;
				best_ind = i;
			}
		}
		else
		{
			if (intersect_triangle(ray, obj, &this_t) && this_t < t_min)
			{
				t_min = this_t;
				best_ind = i;
			}
		}
	}
	*t = t_min;
	return best_ind;
}

float3 trace(Ray ray, __constant Object *scene, int object_count, unsigned int *seed0, unsigned int *seed1)
{

	float3 color = (float3)(0.0f, 0.0f, 0.0f);
	float3 mask = (float3)(1.0f, 1.0f, 1.0f);

	float stop_prob = 0.1f;
	for (int j = 0; j < 8 || get_random(seed0, seed1) <= stop_prob; j++)
	{
		float rrFactor =  j >= 8 ? 1.0 / (1.0 - stop_prob) : 1.0;
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
			N = normalize(hit_point - hit.v0);
		else
			N = normalize(cross(hit.v1 - hit.v0, hit.v2 - hit.v0));
		
		color += mask * hit.emission;

		float3 new_dir;

		//decide what type of bounce we're doing based on material properties
		float prob_max = hit.mat_probabilities.x + hit.mat_probabilities.y + hit.mat_probabilities.z;
		float mat_roll = prob_max * get_random(seed0, seed1);

		if (prob_max == 0.0)
			break ;
		else if (mat_roll < hit.mat_probabilities.x)
		{
			N = dot(ray.direction, N) < 0.0f ? N : N * (-1.0f);
			//local orthonormal system
			float3 axis = fabs(N.x) > fabs(N.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
			float3 hem_x = cross(axis, N);
			float3 hem_y = cross(N, hem_x);

			//generate random direction on the unit hemisphere
			float rsq = get_random(seed0, seed1);
			float r = sqrt(rsq);
			float theta = 2 * PI * get_random(seed0, seed1);

			//combine for new direction
			new_dir = normalize(hem_x * r * cos(theta) + hem_y * r * sin(theta) + N * sqrt(max(0.0f, 1.0f - rsq)));
			mask *= hit.color * dot(new_dir, N) * hit.mat_constants.x * rrFactor * (hit.mat_probabilities.x / prob_max);
			ray.origin = hit_point + N * NORMAL_SHIFT;
		}
		else if (mat_roll < hit.mat_probabilities.y + hit.mat_probabilities.x)
		{
			N = dot(ray.direction, N) < 0.0f ? N : N * (-1.0f);
			new_dir = normalize(ray.direction - 2.0f * N * dot(ray.direction, N));
			mask *= hit.mat_constants.y * rrFactor * (hit.mat_probabilities.y / prob_max);
			ray.origin = hit_point + N * NORMAL_SHIFT;
		}
		else //MAT_REFRACTIVE
		{
			float n = REFRACTIVE_INDEX;
			float R0 = (1.0f - n)/(1.0 + n);
			R0 = R0 * R0;
			if (dot(N, ray.direction) > 0)
			{
				N = -1 * N;
				n = 1.0 / n;
			}
			n = 1.0 / n;

			float cost1 = dot(N, ray.direction) * -1;
			float cost2 = 1.0 - n * n *(1.0 - cost1 * cost1);
			float invc1 = 1.0 - cost1;
			float Rprob = R0 + (1.0 - R0) * invc1 * invc1 * invc1 * invc1 * invc1;
			if (cost2 > 0 && get_random(seed0, seed1) > Rprob)
			{
				new_dir = normalize(ray.direction * n + N * (n * cost1 - sqrt(cost2)));
				ray.origin = hit_point - N * NORMAL_SHIFT;
			}
			else
			{
				new_dir = normalize(ray.direction + N * (cost1 * 2));
				ray.origin = hit_point + N * NORMAL_SHIFT;
			}
			mask *= hit.mat_constants.z * rrFactor * (hit.mat_probabilities.z / prob_max);
		}
		ray.direction = new_dir;
	}
	return color;
}

Ray ray_from_cam(const Camera cam, float x, float y)
{
	Ray ray;
	ray.origin = cam.focus;
	float3 through = cam.origin + cam.d_x * x + cam.d_y * y;
	ray.direction = normalize(cam.focus - through);
	return ray;
}

__kernel void render_kernel(__constant Object *scene, 
							const float3 cam_origin,
							const float3 cam_focus,
							const float3 cam_dx,
							const float3 cam_dy, 
							const int width, 
							const int height, 
							const int obj_count,
							__local float3* sample_pool, 
							__global float3* output,
							__global uint* seeds,
							const uint total_samples)
{
	unsigned int pixel_id = get_group_id(0);
	unsigned int local_id = get_local_id(0);
	unsigned int x = pixel_id % width;
	unsigned int y = pixel_id / width;

	unsigned int xoff = local_id % 16;
	unsigned int yoff = local_id / 16;

	unsigned int seed0 = seeds[pixel_id * 2] + xoff;
	unsigned int seed1 = seeds[pixel_id * 2 + 1] + yoff;

	float x_coord = (float)x + (float)xoff / 16.0f;
	float y_coord = (float)y + (float)yoff / 16.0f;

	float3 sum_color = (float3)(0.0f, 0.0f, 0.0f);

	Camera cam;
	cam.origin = cam_origin;
	cam.focus = cam_focus;
	cam.d_x = cam_dx;
	cam.d_y = cam_dy;

	Ray ray = ray_from_cam(cam, x_coord, y_coord);
	for (int i = 0; i < SAMPLES_PER_LAUNCH / POOLSIZE; i++)
		sum_color += trace(ray, scene, obj_count, &seed0, &seed1) / (float)total_samples;

	sample_pool[local_id] = sum_color;
	seeds[pixel_id * 2] = seed0;
	seeds[pixel_id * 2 + 1] = seed1;
	barrier(CLK_LOCAL_MEM_FENCE);

	//there is a faster way to do this but that's for later
	if (local_id == 0)
	{
		for (int i = 1; i < POOLSIZE; i++)
			sum_color += sample_pool[i];
		output[pixel_id] += sum_color;
	}
}


/*

	TODO:
		material becomes float3, use as probabilities of different types
		probably 2 float3s actually, one for probabilities and one for respective constants
			//this sort of works but i need to explore how people really do it

		bvh on gpu

		render some really sick scenes

		tone mapping is still kinda busted, seems that full black values cause issues
*/