
#define PI 3.14159265359f
#define REFRACTIVE_INDEX 1.5f
#define COLLIDE_ERR 0.0001f
#define NORMAL_SHIFT 0.0003f

#define DIFFUSE_CONSTANT 0.7f
#define SPECULAR_CONSTANT 0.9f

#define SPHERE 1
#define TRIANGLE 3
#define QUAD 4

#define MAT_DIFFUSE 1
#define MAT_SPECULAR 2
#define MAT_REFRACTIVE 3
#define MAT_NULL 4

#define BLACK (float3)(0.0f, 0.0f, 0.0f)
#define WHITE (float3)(1.0f, 1.0f, 1.0f)
#define GREY (float3)(0.5f, 0.5f, 0.5f)

#define SUN (float3)(0.0f, 3000.0f, 0.0f)
#define SUN_BRIGHTNESS 30000.0f

typedef struct s_ray {
	float3 origin;
	float3 direction;
	float3 inv_dir;
}				Ray;

typedef struct s_material
{
	float3 Ka;
	float3 Kd;
	float3 Ks;
	float3 Ke;

	//here's where texture and stuff goes
	int index;
	int height;
	int width;
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
	float minx;
	float miny;
	float minz;
	int lind;
	float maxx;
	float maxy;
	float maxz;
	int rind;
}				Box;

#define NULL_BOX (Box){0.0f, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f, 0};

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

static int inside_box(const float3 pt, const Box box) //currently unused. originally to provide minor speedup in tree traversal
{
	if (box.minx <= pt.x && pt.x <= box.maxx)
		if (box.miny <= pt.y && pt.y <= box.maxy)
			if (box.minz <= pt.z && pt.z <= box.maxz)
				return 1;
	return 0;
}

//intersect_box
static const int intersect_box(const Ray ray, const Box b, float t)
{
	int inside = inside_box(ray.origin, b);
	float tx0 = (b.minx - ray.origin.x) * ray.inv_dir.x;
	float tx1 = (b.maxx - ray.origin.x) * ray.inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	if (!inside && tmin > t)
		return 0;

	float ty0 = (b.miny - ray.origin.y) * ray.inv_dir.y;
	float ty1 = (b.maxy - ray.origin.y) * ray.inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);

	if (!inside && tymin > t)
		return 0;

	if ((tmin >= tymax) || (tymin >= tmax))
		return (0);

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (b.minz - ray.origin.z) * ray.inv_dir.z;
	float tz1 = (b.maxz - ray.origin.z) * ray.inv_dir.z;
	float tzmin = fmin(tz0, tz1);
	float tzmax = fmax(tz0, tz1);

	if (!inside && tzmin > t)
		return 0;

	if ((tmin >= tzmax) || (tzmin >= tmax))
		return (0);

    tmin = fmax(tzmin, tmin);
	tmax = fmin(tzmax, tmax);

	if (tmin <= 0.0f && tmax <= 0.0f)
		return (0);
	return (1);
}

static void intersect_triangle(const Ray ray, __constant float3 *V, int test_i, int *best_i, float *t, float *u, float *v)
{
	//we don't need v1 or v2 after initial calc of e1, e2. could just store them in same memory. wonder if compiler does this.
	float this_t, this_u, this_v;
	const float3 v0 = V[test_i];
	const float3 v1 = V[test_i + 1];
	const float3 v2 = V[test_i + 2];

	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;

	float3 h = cross(ray.direction, e2);
	float a = dot(h, e1);

	if (fabs(a) < 0)
		return;
	float f = 1.0f / a;
	float3 s = ray.origin - v0;
	this_u = f * dot(s, h);
	if (this_u < 0.0f || this_u > 1.0f)
		return;
	float3 q = cross(s, e1);
	this_v = f * dot(ray.direction, q);
	if (this_v < 0.0f || this_u + this_v > 1.0f)
		return;
	this_t = f * dot(e2, q);
	if (this_t < *t && this_t > COLLIDE_ERR)
	{
		*t = this_t;
		*u = this_u;
		*v = this_v;
		*best_i = test_i;
	}
}

static int hit_bvh(	const Ray ray,
					__constant float3 *V,
					__constant Box *boxes,
					float *t_out,
					float *u_out,
					float *v_out)
{

	int stack[32];
	int s_i = 1;
	stack[0] = 0;

	float t = FLT_MAX;
	float u, v;
	int ind = -1;

	Box b;
	while (s_i)
	{
		//pop
		b = boxes[stack[--s_i]];

		//check
		if (intersect_box(ray, b, t))
		{
			//leaf? brute check.
			if (b.rind < 0)
			{
				const int start = -1 * b.lind;
				const int count = -1 * b.rind;
				for (int i = start; i < start + count; i += 3)
					intersect_triangle(ray, V, i, &ind, &t, &u, &v); //will update if success
			}
			else
			{
				stack[s_i++] = b.lind;
				stack[s_i++] = b.rind;
			}
		}
	}

	*t_out = t;
	*u_out = u;
	*v_out = v;
	return ind;
}

//This was KG, needs updated to VNT

// static float3 fetch_color(const float u, const float v, int quadflag, const Object hit, const Material mat, __constant uchar *tex)
// {
// 	if (mat.height == 0 && mat.width == 0)
// 		return mat.Kd;
// 	float3 txcrd;
// 	if(!quadflag)
// 		txcrd = (1 - u - v) * hit.vt[0] + u * hit.vt[1] + v * hit.vt[2];
// 	else
// 		txcrd = (1 - u - v) * hit.vt[0] + u * hit.vt[2] + v * hit.vt[3];

// 	txcrd.x -= floor(txcrd.x);
// 	txcrd.y -= floor(txcrd.y);
// 	if (txcrd.x < 0.0)
// 		txcrd.x = 1.0 + txcrd.x;
// 	if (txcrd.y < 0.0)
// 		txcrd.y = 1.0 + txcrd.y;

// 	int offset = mat.index;
// 	int x = floor((float)mat.width * txcrd.x);
// 	int y = floor((float)mat.height * txcrd.y);

// 	uchar R = tex[offset + (y * mat.width + x) * 3];
// 	uchar G = tex[offset + (y * mat.width + x) * 3 + 1];
// 	uchar B = tex[offset + (y * mat.width + x) * 3 + 2];

// 	float3 out = (float3)((float)R, (float)G, (float)B) / 255.0f * mat.Kd;
// 	return out;
// }

static void fetch_normals(__constant float3 *V, __constant float3 *N, const int ind, const float u, const float v, float3 *sample_N, float3 *geom_N)
{
	float3 v0 = V[ind];
	float3 v1 = V[ind + 1];
	float3 v2 = V[ind + 2];
	*geom_N = normalize(cross(v1 - v0, v2 - v0));

	v0 = N[ind];
	v1 = N[ind + 1];
	v2 = N[ind + 2];
	*sample_N = normalize((1 - u - v) * v0 + u * v1 + v * v2);
}

static float3 trace(Ray ray,
					__constant float3 *V,
					__constant float3 *T,
					__constant float3 *N,
					__constant Box *boxes,
					__constant Material *mats,
					__constant uchar *tex, 
					unsigned int *seed0, 
					unsigned int *seed1)
{

	float3 color = BLACK;
	float3 mask = WHITE;

	const float stop_prob = 0.3f;

	for (int j = 0; j < 8 ; j++)
	{
		float rrFactor = 1.0f;
		
		//collide
		float t, u, v;
		int hit_ind = hit_bvh(ray, V, boxes, &t, &u, &v);

		if (hit_ind == -1)
		{
			// if (j != 0)
			// 	color = mask * SUN_BRIGHTNESS;
			break;
		}

		float3 hit_point = ray.origin + ray.direction * t;

		//get normal at collision point. geom_N is used for the normal_shift step, but might not be necessary.
		float3 sample_N, geom_N;
		fetch_normals(V, N, hit_ind, u, v, &sample_N, &geom_N);

		//flip normals if necessary
		sample_N = dot(ray.direction, sample_N) < 0.0f ? sample_N : sample_N * (-1.0f);
		geom_N = dot(ray.direction, geom_N) < 0.0f ? geom_N : geom_N * (-1.0f);

		// (jank) NEE
		Ray to_sun;
		to_sun.origin = hit_point + sample_N * NORMAL_SHIFT;
		float3 sundir = normalize(SUN - to_sun.origin);
		to_sun.direction = sundir;
		to_sun.inv_dir = 1.0f / sundir;
		float ts, us, vs;
		int sun_hit = hit_bvh(to_sun, V, boxes, &ts, &us, &vs);
		if (sun_hit == -1)
			color += SUN_BRIGHTNESS * mask * dot(-1.0f * ray.direction, sample_N);

		//local orthonormal system
		float3 axis = fabs(sample_N.x) > fabs(sample_N.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
		float3 hem_x = cross(axis, sample_N);
		float3 hem_y = cross(sample_N, hem_x);

		//generate random direction on the unit hemisphere
		float rsq = get_random(seed0, seed1);
		float r = sqrt(rsq);
		float theta = 2 * PI * get_random(seed0, seed1);

		//combine for new direction
		float3 new_dir = normalize(hem_x * r * cos(theta) + hem_y * r * sin(theta) + sample_N * sqrt(max(0.0f, 1.0f - rsq)));
		mask *= dot(new_dir, sample_N) * rrFactor; ////////////// fetch_color goes here
		
		ray.origin = hit_point + geom_N * NORMAL_SHIFT;
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

__kernel void render_kernel(__constant float3 *V,
							__constant float3 *T,
							__constant float3 *N,
							__constant Box *boxes,
							__constant Material *mats,
							__constant uchar *tex,
							const float3 cam_origin,
							const float3 cam_focus,
							const float3 cam_dx,
							const float3 cam_dy,
							const uint sample_count,
							const uint width,
							__constant uint* seeds,
							__global float3* output)
{
	unsigned int pixel_id = get_global_id(0);
	unsigned int x = pixel_id % width;
	unsigned int y = pixel_id / width;

	unsigned int seed0 = seeds[pixel_id * 2];
	unsigned int seed1 = seeds[pixel_id * 2 + 1];

	float3 sum_color = BLACK;

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
		sum_color += trace(ray, V, T, N, boxes, mats, tex, &seed0, &seed1);
	}
	
	output[pixel_id] = sum_color;
}