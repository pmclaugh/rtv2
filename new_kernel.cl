
#define PI 3.14159265359f
#define REFRACTIVE_INDEX 1.5
#define COLLIDE_ERR 0.00001f
#define NORMAL_SHIFT 0.0003f

#define DIFFUSE_CONSTANT 0.7
#define SPECULAR_CONSTANT 0.9

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

#define LUMA (float3)(0.2126f, 0.7152f, 0.0722f)

#define SUN (float3)(0.0f, 10000.0f, 0.0f)
#define SUN_BRIGHTNESS 60000.0f
#define SUN_RAD 1.0f;

#define UNIT_X (float3)(1.0f, 0.0f, 0.0f)
#define UNIT_Y (float3)(0.0f, 1.0f, 0.0f)
#define UNIT_Z (float3)(0.0f, 0.0f, 1.0f)

#define stop_prob 0.3f

typedef struct s_ray {
	float3 origin;
	float3 direction;
	float3 inv_dir;
}				Ray;

typedef struct s_material
{
	float3 Ka;
	float3 Kd;
	float3 Ns;
	float3 Ke;

	//here's where texture and stuff goes
	int d_index;
	int d_height;
	int d_width;

	int s_index;
	int s_height;
	int s_width;

	int b_index;
	int b_height;
	int b_width;

	int t_index;
	int t_height;
	int t_width;
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
static const int intersect_box(const Ray ray, const Box b, const float t)
{
	//the if tmin >=t checks are new, should be fine, should help. will toggle to see effect.

	float tx0 = (b.minx - ray.origin.x) * ray.inv_dir.x;
	float tx1 = (b.maxx - ray.origin.x) * ray.inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	float ty0 = (b.miny - ray.origin.y) * ray.inv_dir.y;
	float ty1 = (b.maxy - ray.origin.y) * ray.inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);


	if ((tmin >= tymax) || (tymin >= tmax))
		return (0);

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (b.minz - ray.origin.z) * ray.inv_dir.z;
	float tz1 = (b.maxz - ray.origin.z) * ray.inv_dir.z;
	float tzmin = fmin(tz0, tz1);
	float tzmax = fmax(tz0, tz1);

	if ((tmin >= tzmax) || (tzmin >= tmax))
		return (0);

    tmin = fmax(tzmin, tmin);
	tmax = fmin(tzmax, tmax);

	if (tmin > t)
		return (0);

	if (tmin <= 0.0 && tmax <= 0.0)
		return (0);
	return (1);
}

static void intersect_triangle(const Ray ray, __global float3 *V, int test_i, int *best_i, float *t, float *u, float *v)
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
	if (this_u < 0.0 || this_u > 1.0)
		return;
	float3 q = cross(s, e1);
	this_v = f * dot(ray.direction, q);
	if (this_v < 0.0 || this_u + this_v > 1.0)
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
					__global float3 *V,
					__global Box *boxes,
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

static float3 fetch_tex(	const float3 txcrd,
							const int offset,
							const int height,
							const int width,
							__global uchar *tex)
{
	int x = floor((float)width * txcrd.x);
	int y = floor((float)height * txcrd.y);

	uchar R = tex[offset + (y * width + x) * 3];
	uchar G = tex[offset + (y * width + x) * 3 + 1];
	uchar B = tex[offset + (y * width + x) * 3 + 2];

	float3 out = ((float3)((float)R, (float)G, (float)B) / 255.0f);
	return out;
}

static void fetch_all_tex(__global Material *mats, const int m_ind, __global uchar *tex, const float3 txcrd, float3 *trans, float3 *bump, float3 *spec, float3 *diff)
{
	Material mat = mats[m_ind];
	*trans = mat.t_height ? fetch_tex(txcrd, mat.t_index, mat.t_height, mat.t_width, tex) : UNIT_X;
	*bump = mat.b_height ? fetch_tex(txcrd, mat.b_index, mat.b_height, mat.b_width, tex) * 2.0f - 1.0f : UNIT_Z;
	*spec = mat.s_height ? fetch_tex(txcrd, mat.s_index, mat.s_height, mat.s_width, tex) : BLACK;
	*diff = mat.d_height ? fetch_tex(txcrd, mat.d_index, mat.d_height,
	mat.d_width, tex) : (float3)(0.3f, 0.8f, 0.8f);
}

static void fetch_NT(__global float3 *V, __global float3 *N, __global float3 *T, const float3 dir, const int ind, const float u, const float v, float3 *N_out, float3 *txcrd_out)
{
	float3 v0 = V[ind];
	float3 v1 = V[ind + 1];
	float3 v2 = V[ind + 2];
	float3 geom_N = normalize(cross(v1 - v0, v2 - v0));

	v0 = N[ind];
	v1 = N[ind + 1];
	v2 = N[ind + 2];
	float3 sample_N = normalize((1.0f - u - v) * v0 + u * v1 + v * v2);

	*N_out = dot(dir, geom_N) <= 0.0f ? sample_N : -1.0f * sample_N;

	float3 txcrd = (1.0f - u - v) * T[ind] + u * T[ind + 1] + v * T[ind + 2];
	txcrd.x -= floor(txcrd.x);
	txcrd.y -= floor(txcrd.y);
	if (txcrd.x < 0.0f)
		txcrd.x = 1.0f + txcrd.x;
	if (txcrd.y < 0.0f)
		txcrd.y = 1.0f + txcrd.y;
	*txcrd_out = txcrd;	
}

static float3 bump_map(__global float3 *TN, __global float3 *BTN, const int ind, const float3 sample_N, const float3 bump)
{
	float3 tangent = TN[ind];
	float3 bitangent = BTN[ind];
	tangent = normalize(tangent - sample_N * dot(sample_N, tangent));
	return normalize(tangent * bump.x + bitangent * bump.y + sample_N * bump.z);
}

static float3 trace(Ray ray,
					__global float3 *V,
					__global float3 *T,
					__global float3 *N,
					__global Box *boxes,
					__global Material *mats,
					__global uchar *tex, 
					unsigned int *seed0, 
					unsigned int *seed1,
					__global int *M,
					__global float3 *TN,
					__global float3 *BTN)
{

	float3 color = BLACK;
	float3 mask = WHITE;

	for (int j = 0; j < 5 || get_random(seed0, seed1) < stop_prob; j++)
	{
		//collide
		float t, u, v;
		const int hit_ind = hit_bvh(ray, V, boxes, &t, &u, &v);

		if (hit_ind == -1)
		{
			color = mask * SUN_BRIGHTNESS;
			break;
		}

		//get normal at collision point. geom_N is used for the normal_shift step, but might not be necessary.
		float3 sample_N, txcrd;
		fetch_NT(V, N, T, ray.direction, hit_ind, u, v, &sample_N, &txcrd);

		//get material data
		float3 trans, bump, spec, diff;
		fetch_all_tex(mats, M[hit_ind / 3], tex, txcrd, &trans, &bump, &spec, &diff);

		if (trans.x < 1.0f)
		{
			ray.origin = ray.origin + ray.direction * (t + NORMAL_SHIFT);
			j--;
			continue;
		}

		sample_N = bump_map(TN, BTN, hit_ind / 3, sample_N, bump);
		
		mask *= j >= 5 ? 1.0f / (1.0f - stop_prob) : 1.0f;
		float spec_importance = spec.x + spec.y + spec.z;
		float diff_importance = diff.x + diff.y + diff.z;
		float total = spec_importance + diff_importance;
		spec_importance /= total;
		diff_importance /= total;
		float3 new_dir;
		float r1 = get_random(seed0, seed1);
		float r2 = get_random(seed0, seed1);
		if(get_random(seed0, seed1) < spec_importance)
		{
			float3 spec_dir = normalize(ray.direction - 2.0f * dot(ray.direction, sample_N) * sample_N);

			//local orthonormal system
			float3 axis = fabs(spec_dir.x) > fabs(spec_dir.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
			float3 hem_x = cross(axis, spec_dir);
			float3 hem_y = cross(spec_dir, hem_x);

			float phi = 2.0f * PI * r1;
			float theta = acos(pow((1.0f - r2), 1.0f / (100.0f * spec.x)));

			float3 x = hem_x * sin(theta) * cos(phi);
			float3 y = hem_y * sin(theta) * sin(phi);
			float3 z = spec_dir * cos(theta);
			new_dir = x + y + z;
			if (dot(new_dir, sample_N) < 0.0f) // pick mirror of sample (same importance)
				new_dir = z - x - y;
			mask *= spec;
		}
		else
		{
			//Diffuse reflection (default)
			//local orthonormal system
			float3 axis = fabs(sample_N.x) > fabs(sample_N.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
			float3 hem_x = cross(axis, sample_N);
			float3 hem_y = cross(sample_N, hem_x);

			//generate random direction on the unit hemisphere (cosine-weighted for importance sampling)
			float r = sqrt(r1);
			float theta = 2 * PI * r2;

			//combine for new direction
			new_dir = normalize(hem_x * r * cos(theta) + hem_y * r * sin(theta) + sample_N * sqrt(max(0.0f, 1.0f - r1)));
			mask *= diff;
		}

		ray.origin = ray.origin + ray.direction * t + sample_N * NORMAL_SHIFT;
		ray.direction = new_dir;
		ray.inv_dir = 1.0f / new_dir;

	}
	return color;
}

#define DOF_rad 0.0f

static Ray ray_from_cam(const Camera cam, float x, float y, uint *s0, uint *s1)
{
	Ray ray;
	ray.origin = cam.focus;
	float3 through = cam.origin + cam.d_x * x + cam.d_y * y;
	ray.direction = normalize(cam.focus - through);
	ray.inv_dir = 1.0f / ray.direction;
	return ray;
}

__kernel void render_kernel(__global float3 *V,
							__global float3 *T,
							__global float3 *N,
							__global Box *boxes,
							__global Material *mats,
							__global uchar *tex,
							const float3 cam_origin,
							const float3 cam_focus,
							const float3 cam_dx,
							const float3 cam_dy,
							const uint sample_count,
							const uint width,
							__global uint* seeds,
							__global float3* output,
							__global int *M,
							__global float3 *TN,
							__global float3 *BTN)
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
		Ray ray = ray_from_cam(cam, x_coord, y_coord, &seed0, &seed1);
		sum_color += trace(ray, V, T, N, boxes, mats, tex, &seed0, &seed1, M, TN, BTN);
	}
	
	output[pixel_id] = sum_color;
}
