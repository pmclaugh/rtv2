
__constant float PI = 3.14159265359f;
__constant float REFRACTIVE_INDEX = 1.5;
__constant float COLLIDE_ERR = 0.00001f;
__constant float NORMAL_SHIFT = 0.00003f;

__constant float DIFFUSE_CONSTANT = 0.7;
__constant float SPECULAR_CONSTANT = 0.9;

#define SPHERE 1
#define TRIANGLE 3
#define QUAD 4

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
	float3 inv_dir;
}				Ray;

typedef struct s_object
{
	int shape;
	int mat_type;
	int mat_ind;
	int smoothing;
	float3 v[4];
	float3 vn[4]; //texturing normals (from file)
	float3 vt[4];
	float3 N; //literal geometric normal
}				Object;

typedef struct s_material
{
	float3 Ka;
	float3 Kd;
	float3 Ks;
	float3 Ke;

	//here's where texture and stuff goes
}				Material;

typedef struct s_camera
{
	float3 origin;
	float3 focus;
	float3 d_x;
	float3 d_y;
}				Camera;

typedef struct s_box
{
	float3 min;
	float3 max; 
	int start;
	int end;
	int children[8];
	int children_count;
}				Box;

typedef struct compact_box
{
	float3 min;
	float3 max;
	int key;
}				C_Box;

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

static const int intersect_sphere(const Ray ray, const Object sph, float *t)
{
	float3 toCenter = sph.v[0] - ray.origin;
	float b = dot(toCenter, ray.direction);
	float c = dot(toCenter, toCenter) - sph.v[1].x * sph.v[1].x;
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

static const int intersect_triangle(const Ray ray, const Object tri, float *t, float *u, float *v)
{
	float3 e1 = tri.v[1] - tri.v[0];
	float3 e2 = tri.v[2] - tri.v[0];

	float3 h = cross(ray.direction, e2);
	float a = dot(h, e1);

	if (fabs(a) < 0)
		return 0;
	float f = 1.0f / a;
	float3 s = ray.origin - tri.v[0];
	*u = f * dot(s, h);
	if (*u < 0.0 || *u > 1.0)
		return 0;
	float3 q = cross(s, e1);
	*v = f * dot(ray.direction, q);
	if (*v < 0.0 || *u + *v > 1.0)
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

static const int intersect_triangle_raw(const Ray ray, const float3 v0, const float3 v1, const float3 v2, float *t, float *u, float *v)
{
	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;

	float3 h = cross(ray.direction, e2);
	float a = dot(h, e1);

	if (fabs(a) < 0)
		return 0;
	float f = 1.0f / a;
	float3 s = ray.origin - v0;
	*u = f * dot(s, h);
	if (*u < 0.0 || *u > 1.0)
		return 0;
	float3 q = cross(s, e1);
	*v = f * dot(ray.direction, q);
	if (*v < 0.0 || *u + *v > 1.0)
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

static int inside_box(const float3 pt, const Box box, float *t)
{
	if (box.min.x <= pt.x && pt.x <= box.max.x)
		if (box.min.y <= pt.y && pt.y <= box.max.y)
			if (box.min.z <= pt.z && pt.z <= box.max.z)
			{
				*t = 0.0f;
				return 1;
			}
	return 0;
}

static int inside_bounds(const float3 pt, const float3 bmin, const float3 bmax, float *t)
{
	if (bmin.x <= pt.x && pt.x <= bmax.x)
		if (bmin.y <= pt.y && pt.y <= bmax.y)
			if (bmin.z <= pt.z && pt.z <= bmax.z)
			{
				*t = 0.0f;
				return 1;
			}
	return 0;
}

//intersect_box
static const int intersect_box(const Ray ray, const float3 bmin, const float3 bmax, float *t_out)
{	
	float tx0 = (bmin.x - ray.origin.x) * ray.inv_dir.x;
	float tx1 = (bmax.x - ray.origin.x) * ray.inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	float ty0 = (bmin.y - ray.origin.y) * ray.inv_dir.y;
	float ty1 = (bmax.y - ray.origin.y) * ray.inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);

	if ((tmin >= tymax) || (tymin >= tmax))
		return (0);

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (bmin.z - ray.origin.z) * ray.inv_dir.z;
	float tz1 = (bmax.z - ray.origin.z) * ray.inv_dir.z;
	float tzmin = fmin(tz0, tz1);
	float tzmax = fmax(tz0, tz1);

	if ((tmin >= tzmax) || (tzmin >= tmax))
		return (0);

    tmin = fmax(tzmin, tmin);
	tmax = fmin(tzmax, tmax);
	if (tmin <= 0.0 && tmax <= 0.0)
		return (0);
	if (tmin > 0.0)
		*t_out = tmin;
	else
		*t_out = tmax;
	return (1);
}


//intersect_object
static const int intersect_object(const Ray ray, const Object obj, float *t, float *u_out, float *v_out, int *qf)
{
	if (obj.shape == SPHERE)
		return intersect_sphere(ray, obj, t);
	else if (obj.shape == TRIANGLE)
	{
		float u, v;
		*qf = 0;
		return intersect_triangle(ray, obj, t, &u, &v);
	}
	else if (obj.shape == QUAD)
	{
		int result_a, result_b;
		float t_a = FLT_MAX;
		float t_b = FLT_MAX;
		float ua, va, ub, vb;
		result_a = intersect_triangle_raw(ray, obj.v[0], obj.v[1], obj.v[2], &t_a, &ua, &va);
		result_b = intersect_triangle_raw(ray, obj.v[0], obj.v[3], obj.v[2], &t_b, &ub, &vb);
		if (result_a && result_b)
		{
			if (t_a < t_b)
			{
				*t = t_a;
				*u_out = ua;
				*v_out = va;
				*qf = 0;
			}
			else
			{
				*t = t_b;
				*u_out = ub;
				*v_out = vb;
				*qf = 1;
			}
			return (1);
		}
		else if (result_a)
		{
			*t = t_a;
			*u_out = ua;
			*v_out = va;
			*qf = 0;
			return (1);
		}
		else if (result_b)
		{
			*t = t_b;
			*u_out = ub;
			*v_out = vb;
			*qf = 1;
			return (1);
		}
		else
			return (0);
	}
	else
		return (0);
}

static int hit_bvh(const Ray ray, __constant Object *scene, __constant Box *boxes, int index, float *t_out, float *u_out, float *v_out, int *qf_out)
{

	int2 stack[32];
	int best_ind = -1;
	float best_t = FLT_MAX;
	float bu, bv;
	int qf;

	float box_t;
	stack[0][0] = index;
	stack[0][1] = 0;
	int count = 1;
	
	while (count)
	{
		int tail_ind = stack[count - 1][0];
		int child_ind = stack[count - 1][1];
		stack[count - 1][1] = stack[count - 1][1] + 1;
		const Box tail = boxes[tail_ind];

		if (child_ind == tail.children_count)
		{
			count--;
			continue;
		}

		int candidate_ind = tail.children[child_ind];
		const Box candidate = boxes[candidate_ind];
		if (inside_box(ray.origin, candidate, &box_t) || intersect_box(ray, candidate.min, candidate.max, &box_t))
		{
			if (box_t <= best_t)
			{
				if (candidate.children_count)
				{
					stack[count][0] = boxes[tail_ind].children[child_ind];
					stack[count][1] = 0;
					count++;
				}
				else //if leaf, try intersect faces
				{
					float t, u, v;
					int quadflag;
					for (int i = candidate.start; i < candidate.end; i++)
					{
						Object obj = scene[i];
						if (intersect_object(ray, obj, &t, &u, &v, &quadflag))
							if (t < best_t)
							{
								best_t = t;
								best_ind = i;
								bu = u;
								bv = v;
								qf = quadflag;
							}
					}
				}
			}
		}
	}
	*t_out = best_t;
	*u_out = bu;
	*v_out = bv;
	*qf_out = qf;
	return best_ind;
}

static int hit_meta_bvh(const Ray ray, __constant Object *scene, __constant Box *boxes, __constant C_Box *object_boxes, const uint obj_count, float *t_out, float *u_out, float *v_out, int *qf_out)
{
	//this is a temporary test implementation, meta bvh is just a list. showed 2x speedup even this way. very promising.
	float best_t = FLT_MAX;
	int best_ind = -1;
	float bu, bv;
	int qf;
	for (int i = 0; i < obj_count; i++)
	{
		float t;
		C_Box cb = object_boxes[i];
		if (inside_bounds(ray.origin, cb.min, cb.max, &t) || intersect_box(ray, cb.min, cb.max, &t))
		{
			if (t < best_t)
			{
				float u, v;
				int quadflag;
				int ret = hit_bvh(ray, scene, boxes, cb.key, &t, &u, &v, &quadflag);
				if (ret != -1 && t < best_t)
				{
					best_t = t;
					best_ind = ret;
					bu = u;
					bv = v;
					qf = quadflag;
				}
			}
		}
	}
	*u_out = bu;
	*v_out = bv;
	*t_out = best_t;
	*qf_out = qf;
	return best_ind;
}

static float3 trace(Ray ray, __constant Object *scene, __constant Material *mats, __constant Box *boxes, __constant C_Box *object_boxes, const uint obj_count, unsigned int *seed0, unsigned int *seed1)
{

	float3 color = BLACK;
	float3 mask = WHITE;

	const float stop_prob = 0.2f;

	for (int j = 0; j < 5 || get_random(seed0, seed1) <= stop_prob; j++)
	{
		float rrFactor = j >= 5 ? 1.0f / (1.0f - stop_prob) : 1.0;
		
		//collide
		float t, u, v;
		int quadflag;
		int hit_ind = hit_meta_bvh(ray, scene, boxes, object_boxes, obj_count, &t, &u, &v, &quadflag);
		if (hit_ind == -1)
		{
			if (j != 0)
				color += 100.0f * mask;
			break ;
		}
		const Object hit = scene[hit_ind];
		const Material mat = mats[hit.mat_ind];

		float3 hit_point = ray.origin + ray.direction * t;

		//get normal at collision point
		float3 N;
		float3 VT;
		if (hit.shape == SPHERE)
			N = normalize(hit_point - hit.v[0]);
		else if (hit.shape == TRIANGLE)
		{
			N = u * hit.vn[0] + v * hit.vn[1] + (1 - u - v) * hit.vn[2];
			VT = u * hit.vt[0] + v * hit.vt[1] + (1 - u - v) * hit.vt[2];
		}
		else if (hit.shape == QUAD)
		{
			if (quadflag)
			{
				N = u * (hit.vn[0] + hit.vn[1]) / 2.0 + v * hit.vn[3] + (1 - u - v) * (hit.vn[2] + hit.vn[1]) / 2.0;
				VT = u * (hit.vt[0] + hit.vt[1]) / 2.0 + v * hit.vt[3] + (1 - u - v) * (hit.vt[2] + hit.vt[1]) / 2.0;
			}
			else
			{
				N = u * (hit.v[0] + hit.vn[3]) / 2.0 + v * hit.vn[1] + (1 - u - v) * (hit.vn[2] + hit.vn[3]) / 2.0;
				VT = u * (hit.vt[0] + hit.vt[3]) / 2.0 + v * hit.vt[1] + (1 - u - v) * (hit.vt[2] + hit.vt[3]) / 2.0;
			}
		}

		float3 new_dir;

		if (hit.mat_type == MAT_DIFFUSE)
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
			mask *= dot(new_dir, N) * mat.Kd * rrFactor; //////////////
			ray.origin = hit_point + N * NORMAL_SHIFT;
		}
		else if (hit.mat_type == MAT_SPECULAR)
		{
			N = dot(ray.direction, N) < 0.0f ? N : N * (-1.0f);
			new_dir = normalize(ray.direction - 2.0f * N * dot(ray.direction, N));
			mask *= SPECULAR_CONSTANT * rrFactor;
			ray.origin = hit_point + N * NORMAL_SHIFT;
		}
		else if (hit.mat_type == MAT_REFRACTIVE)
		{
			break ; //disabled for now
		}
		else // MAT_NULL
		{
			//we hit the light
			color += (10.0f * mask);
			break ;
		}

		ray.direction = new_dir;
		ray.inv_dir = 1.0f / new_dir;
	}
	return color;
}

static Ray ray_from_cam(const Camera cam, float x, float y)
{
	Ray ray;
	ray.origin = cam.focus;
	float3 through = cam.origin + cam.d_x * x + cam.d_y * y;
	ray.direction = normalize(cam.focus - through);
	ray.inv_dir = 1.0f / ray.direction;
	return ray;
}

__kernel void render_kernel(__constant Object *scene,
							__constant Material *mats,
							__constant Box *boxes,
							const float3 cam_origin,
							const float3 cam_focus,
							const float3 cam_dx,
							const float3 cam_dy, 
							const int width,
							__global float3* output,
							__global uint* seeds,
							const uint sample_count,
							__constant C_Box *object_boxes,
							const uint obj_count)
{
	unsigned int pixel_id = get_global_id(0);
	unsigned int x = pixel_id % width;
	unsigned int y = pixel_id / width;

	unsigned int seed0 = seeds[pixel_id * 2];
	unsigned int seed1 = seeds[pixel_id * 2 + 1];

	float3 sum_color = (float3)(0.0f, 0.0f, 0.0f);

	Camera cam;
	cam.origin = cam_origin;
	cam.focus = cam_focus;
	cam.d_x = cam_dx;
	cam.d_y = cam_dy;

	for (int i = 0; i < sample_count; i++)
	{
		float x_coord = (float)x + get_random(&seed0, &seed1);
		float y_coord = (float)y + get_random(&seed0, &seed1);
		Ray ray = ray_from_cam(cam, x_coord, y_coord);
		sum_color += trace(ray, scene, mats, boxes, object_boxes, obj_count, &seed0, &seed1) / (float)sample_count;
	}

	seeds[pixel_id * 2] = seed0;
	seeds[pixel_id * 2 + 1] = seed1;
	output[pixel_id] += sum_color;
}