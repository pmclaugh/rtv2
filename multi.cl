
#define PI 3.14159265359f
#define NORMAL_SHIFT 0.0001f
#define COLLISION_ERROR 0.0003f

#define BLACK (float3)(0.0f, 0.0f, 0.0f)
#define WHITE (float3)(1.0f, 1.0f, 1.0f)
#define GREY (float3)(0.5f, 0.5f, 0.5f)

#define LUMA (float3)(0.2126f, 0.7152f, 0.0722f)

#define SUN (float3)(0.0f, 10000.0f, 0.0f)
#define SUN_BRIGHTNESS 6000.0f
#define SUN_RAD 1.0f;

#define UNIT_X (float3)(1.0f, 0.0f, 0.0f)
#define UNIT_Y (float3)(0.0f, 1.0f, 0.0f)
#define UNIT_Z (float3)(0.0f, 0.0f, 1.0f)

#define MIN_BOUNCES 5
#define RR_PROB 0.3f

#define NEW 0
#define TRAVERSE 1
#define FETCH 2
#define BOUNCE 3
#define DEAD 4

typedef struct s_ray {
	float3 origin;
	float3 direction;
	float3 inv_dir;

	float3 diff;
	float3 spec;
	float3 trans;
	float3 bump;
	float3 N;
	float t;
	float u;
	float v;

	float3 color;
	float3 mask;

	int hit_ind;
	int pixel_id;
	int bounce_count;
	int status;
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
	int width;
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

//intersect_box
static inline int intersect_box(Ray ray, Box b, float t, float *t_out)
{
	float tx0 = (b.minx - ray.origin.x) * ray.inv_dir.x;
	float tx1 = (b.maxx - ray.origin.x) * ray.inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	float ty0 = (b.miny - ray.origin.y) * ray.inv_dir.y;
	float ty1 = (b.maxy - ray.origin.y) * ray.inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);


	if ((tmin > tymax) || (tymin > tmax))
		return (0);

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (b.minz - ray.origin.z) * ray.inv_dir.z;
	float tz1 = (b.maxz - ray.origin.z) * ray.inv_dir.z;
	float tzmin = fmin(tz0, tz1);
	float tzmax = fmax(tz0, tz1);

	if ((tmin > tzmax) || (tzmin > tmax))
		return (0);

    tmin = fmax(tzmin, tmin);
	tmax = fmin(tzmax, tmax);

	if (tmin >= t)
		return (0);
	if (tmin <= 0.0f && tmax <= 0.0f)
		return (0);
	if (t_out)
		*t_out = fmax(0.0f, tmin);
	return (1);
}

static inline void intersect_triangle(Ray ray, __global float3 *V, int test_i, int *best_i, float *t, float *u, float *v)
{
	float this_t, this_u, this_v;
	float3 v0 = V[test_i];
	float3 v1 = V[test_i + 1];
	float3 v2 = V[test_i + 2];

	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;

	float3 h = cross(ray.direction, e2);
	float a = dot(h, e1);

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
	if (this_t < *t && this_t > COLLISION_ERROR)
	{
		*t = this_t;
		*u = this_u;
		*v = this_v;
		*best_i = test_i;
	}
}

static float3 fetch_tex(	float3 txcrd,
							int offset,
							int height,
							int width,
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

static void fetch_all_tex(__global Material *mats, int m_ind, __global uchar *tex, float3 txcrd, float3 *trans, float3 *bump, float3 *spec, float3 *diff)
{
	Material mat = mats[m_ind];
	*trans = mat.t_height ? fetch_tex(txcrd, mat.t_index, mat.t_height, mat.t_width, tex) : UNIT_X;
	*bump = mat.b_height ? fetch_tex(txcrd, mat.b_index, mat.b_height, mat.b_width, tex) * 2.0f - 1.0f : UNIT_Z;
	*spec = mat.s_height ? fetch_tex(txcrd, mat.s_index, mat.s_height, mat.s_width, tex) : mat.Ka;
	*diff = mat.d_height ? fetch_tex(txcrd, mat.d_index, mat.d_height, mat.d_width, tex) : mat.Kd;
}

static void fetch_NT(__global float3 *V, __global float3 *N, __global float3 *T, float3 dir, int ind, float u, float v, float3 *N_out, float3 *txcrd_out)
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

static float3 bump_map(__global float3 *TN, __global float3 *BTN, int ind, float3 sample_N, float3 bump)
{
	float3 tangent = TN[ind];
	float3 bitangent = BTN[ind];
	tangent = normalize(tangent - sample_N * dot(sample_N, tangent));
	return normalize(tangent * bump.x + bitangent * bump.y + sample_N * bump.z);
}

__kernel void fetch(	__global Ray *rays,
						__global float3 *V,
						__global float3 *T,
						__global float3 *N,
						__global float3 *TN,
						__global float3 *BTN,
						__global Material *mats,
						__global int *M,
						__global uchar *tex)
{
	//pull ray and populate its sample_N, diff, spec, trans
	int gid = get_global_id(0);
	Ray ray = rays[gid];

	if (ray.status != FETCH)
		return ;

	float3 txcrd;
	fetch_NT(V, N, T, ray.direction, ray.hit_ind, ray.u, ray.v, &ray.N, &txcrd);
	ray.N = bump_map(TN, BTN, ray.hit_ind / 3, ray.N, ray.bump);
	fetch_all_tex(mats, M[ray.hit_ind / 3], tex, txcrd, &ray.trans, &ray.bump, &ray.spec, &ray.diff);
	ray.status = BOUNCE;

	if (ray.trans.x < 1.0)
	{
		ray.origin = ray.origin + ray.direction * (ray.t + NORMAL_SHIFT);
		ray.bounce_count--;
		ray.status = TRAVERSE;
	}

	rays[gid] = ray;
}

/////GGX stuff

static float GGX_eval(const float3 i, const float3 o, const float3 m, const float3 n, const float a, const float n1, const float n2)
{
	// == F * G * D / 4 |i dot n| |o dot n|
	float denom = 4.0f * fmax(fabs(dot(i, n)), 0.00001f) * fmax(fabs(dot(o, n)), 0.00001f);

	float f = GGX_F(i, m, n1, n2);
	//printf("f is %f\n", f);
	float g = GGX_G(i, o, m, n, a);
	float d = GGX_D(a, dot(m,n));

	if (f != f)
		printf("f nan\n");
	if (f == 0.0f)
		return 0.0f;
	if (g != g)
		printf("g1 nan\n");
	if (g == 0.0f)
		return 0.0f;
	if (d != d)
		printf("d nan, inputs were %f %f\n", a, dot(m, n));
	if (d == 0.0f)
		return 0.0f;

	float eval = f * g * d / denom;
	if (eval != eval)
		printf("eval nan, f %f g %f d %f denom %f\n", f, g, d, denom);

	//printf("f %f g %f d %f / denom %f, eval %f\n", f, g, d, denom, f * g * d / denom);

	return eval; //
}

static float3 GGX_NDF(float3 i, float3 n, float r1, float r2, float a)
{
	//return a direction m with relative probability GGX_D(m)|m dot n|
	float r1 = get_random(seed0, seed1);
	float r2 = get_random(seed0, seed1);
	float theta = atan(a * sqrt(r1) * rsqrt(1 - r1));
	float phi = 2.0f * PI * r2;
	
	//local orthonormal system
	float3 axis = fabs(n.x) > fabs(n.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
	float3 hem_x = cross(axis, n);
	float3 hem_y = cross(n, hem_x);

	float3 x = hem_x * native_sin(theta) * native_cos(phi);
	float3 y = hem_y * native_sin(theta) * native_sin(phi);
	float3 z = n * native_cos(theta);

	float3 m = normalize(x + y + z);
	if (dot(i,m) < 0.0f)
			m = normalize(z - x - y);
	return m;
}

static float GGX_D(const float a, const float cost)
{
	if (1.0f - cost < 0.001)
		return 1.0f;
	float a2 = a * a;
	float chi = cost > 0.0f ? 1.0f : 0.0f;
	if (chi == 0.0f)
		return 0.0f;
	float num = a * chi;

	float theta = acos(cost);
	float cos4 = cost * cost * cost * cost;
	float tsq = a + tan(theta) * tan(theta);
	tsq *= tsq;
	float denom = PI * cos4 * tsq;

	return num == 0.0f ? 0.0f : num / denom;
}

static float GGX_G1(const float3 v, const float3 m, const float3 n, const float a)
{
	float chi = dot(v,m) / dot(v,n) > 0.0f ? 1.0f : 0.0f;
	if (chi == 0.0f)
		return 0.0f;

	float theta = acos(dot(v, m));
	float radicand = 1.0f + a * a * tan(theta) * tan(theta);

	float ret = 2.0f / (1.0f + sqrt(radicand));

	return ret;
}

static float GGX_G(const float3 i, const float3 o, const float3 m, const float3 n, const float a)
{
	if (dot(i,m) * dot(i,n) <= 0.0f || dot(o,m) * dot(o,n) <= 0.0f)
		return 0.0f;
	float g1 = GGX_G1(i, m, n, a);
	float g2 = GGX_G1(o, m, n, a);
	if (g1 != g1)
		printf("g1 nan\n");
	if (g1 == 0.0f)
		return 0.0f;
	if (g2 != g2)
		printf("g2 nan\n");
	if (g2 == 0.0f)
		return 0.0f;
	return g1 * g2;
}

static float GGX_F(const float3 i, const float3 m, const float n1, const float n2)
{
	float c = dot(i, m);
	//printf("c is %f\n", c);
	float radicand = n2 / n1 - 1 + c * c;
	if (radicand <= 0.0001f)
		return 1.0f;
	float g = sqrt(radicand);

	float gminc = g - c;
	float gplusc = g + c;

	float top = c * gplusc - 1;
	top *= top;
	float bot = c * gminc + 1;
	bot *= bot;

	return 0.5f * (gminc * gminc) / (gplusc * gplusc) * (1.0f + top / bot);
}

static float GGX_weight(float3 i, float3 o, float3 m, float3 n, float a)
{
	float num = dot(i,m) * GGX_G(i, o, m, n, a);
	if (num == 0.0f)
		return 1.0f; //safe because G term in numerator of Fs will also be zero
	float denom = dot(i, n) * dot(m, n);
	float weight = num / denom;
	// printf("weight %f\n", weight);

	return num / denom;
}

__kernel void bounce( 	__global Ray *rays,
						__global uint *seeds)
{
	int gid = get_global_id(0);
	Ray ray = rays[gid];
	if (ray.status != BOUNCE)
		return;

	uint seed0 = seeds[2 * gid];
	uint seed1 = seeds[2 * gid + 1];

	// spec_importance = ray.spec.x;
	// diff_importance = 1 - spec_importance;

	// float3 new_dir;
	// float r1 = get_random(&seed0, &seed1);
	// float r2 = get_random(&seed0, &seed1);

	

	// if(get_random(&seed0, &seed1) < spec_importance)
	// {
	// 	float3 spec_dir = normalize(ray.direction - 2.0f * dot(ray.direction, ray.N) * ray.N);

	// 	//local orthonormal system
	// 	float3 axis = fabs(spec_dir.x) > fabs(spec_dir.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
	// 	float3 hem_x = cross(axis, spec_dir);
	// 	float3 hem_y = cross(spec_dir, hem_x);

	// 	float phi = 2.0f * PI * r1;
	// 	float theta = acos(native_powr((1.0f - r2), 1.0f / (50.0f)));

	// 	float3 x = hem_x * native_sin(theta) * native_cos(phi);
	// 	float3 y = hem_y * native_sin(theta) * native_sin(phi);
	// 	float3 z = spec_dir * native_cos(theta);
	// 	new_dir = normalize(x + y + z);
	// 	if (dot(new_dir, ray.N) < 0.0f) // pick mirror of sample (same importance)
	// 		new_dir = normalize(z - x - y);
	// 	ray.mask *= spec_importance > 0 ? ray.spec / spec_importance : 0;
	// }

	// float spec_importance = ray.spec.x + ray.spec.y + ray.spec.z;
	// float diff_importance = ray.diff.x + ray.diff.y + ray.diff.z;
	// float total = spec_importance + diff_importance;
	// spec_importance /= total;
	// diff_importance /= total;

	// float3 new_dir;
	// float r1 = get_random(&seed0, &seed1);
	// float r2 = get_random(&seed0, &seed1);

	float spec_importance = ray.spec.x;
	float diff_importance = 1 - spec_importance;

	if(1 || get_random(&seed0, &seed1) < spec_importance)
	{
		//let's just hardcode these for now
		float a = 0.8f;
		float n1 = 1.0f;
		float n2 = 2.0f;

		float3 n = ray.N;
		float3 i = ray.direction * -1.0f;
		float3 m = GGX_NDF(i, n, r1, r2, a); //sampling microfacet normal
		float3 o = normalize(ray.direction - 2.0f * dot(ray.direction, m) * m); //generate specular direction based on m
		
		new_dir = o;

		float eval = GGX_eval(i, o, m, n, a, n1, n2);
		float weight = GGX_weight(i, o, m, n, a);

		ray.mask *= spec_importance > 0 ? (ray.spec * eval * weight) / (spec_importance) : 0;
	}
	else
	{
		//Diffuse reflection (default)
		//local orthonormal system
		float3 axis = fabs(ray.N.x) > fabs(ray.N.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
		float3 hem_x = cross(axis, ray.N);
		float3 hem_y = cross(ray.N, hem_x);

		//generate random direction on the unit hemisphere (cosine-weighted fo)
		float r = native_sqrt(r1);
		float theta = 2 * PI * r2;

		//combine for new direction
		new_dir = normalize(hem_x * r * native_cos(theta) + hem_y * r * native_sin(theta) + ray.N * native_sqrt(max(0.0f, 1.0f - r1)));
		ray.mask *= diff_importance > 0 ? ray.diff / diff_importance : 0;
	}

	ray.origin = ray.origin + ray.direction * ray.t + ray.N * NORMAL_SHIFT;
	ray.direction = new_dir;
	ray.inv_dir = 1.0f / new_dir;
	ray.status = TRAVERSE;

	rays[gid] = ray;
	seeds[2 * gid] = seed0;
	seeds[2 * gid + 1] = seed1;
}

__kernel void collect(	__global Ray *rays,
						__global uint *seeds,
						const Camera cam,
						__global float3 *output,
						__global int *sample_counts,
						const int sample_max)
{
	int gid = get_global_id(0);
	Ray ray = rays[gid];
	uint seed0 = seeds[2 * gid];
	uint seed1 = seeds[2 * gid + 1];

	if (ray.status == TRAVERSE)
	{
		//update bounce count, do RR if needed
		ray.bounce_count++;
		if (ray.bounce_count > MIN_BOUNCES)
		{
			if (get_random(&seed0, &seed1) < RR_PROB)
				ray.mask = ray.mask / (1.0f - RR_PROB);
			else
				ray.status = DEAD;
		}
	}
	if (ray.status == DEAD)
	{
		if (ray.hit_ind == -1)
		{
			if (ray.bounce_count != 0)
				output[ray.pixel_id] += ray.mask * SUN_BRIGHTNESS;// * pow(fmax(0.0f, dot(ray.direction, UNIT_Y)), 5.0f);
			else
				output[ray.pixel_id] += 0.1f * SUN_BRIGHTNESS;
		}
		sample_counts[ray.pixel_id] += 1;
		ray.status = NEW;
	}
	if (ray.status == NEW)
	{
		float x = (float)(gid % cam.width) + get_random(&seed0, &seed1);
		float y = (float)(gid / cam.width) + get_random(&seed0, &seed1);
		ray.origin = cam.focus;
		float3 through = cam.origin + cam.d_x * x + cam.d_y * y;
		ray.direction = normalize(cam.focus - through);
		ray.inv_dir = 1.0f / ray.direction;
		
		ray.status = TRAVERSE;
		ray.color = BLACK;
		ray.mask = WHITE;
		ray.bounce_count = 0;
		ray.pixel_id = gid;
	}

	rays[gid] = ray;
	seeds[2 * gid] = seed0;
	seeds[2 * gid + 1] = seed1;
}

__kernel void traverse(	__global Ray *rays,
						__global Box *boxes,
						__global float3 *V,
						__constant Box *boost,
						const int boost_count)
{
	int gid = get_global_id(0);
	Ray ray = rays[gid];

	if (ray.status != TRAVERSE)
		return ;

	float t = FLT_MAX;
	float u, v;
	int ind = -1;

	int stack[32];
	stack[0] = 0;
	int s_i = 1;
  
	while (s_i)
	{
		int b_i = stack[--s_i];
		Box b;

		if (b_i < 2048)
			b = boost[b_i];
		else
			b = boxes[b_i];

		//check
		if (intersect_box(ray, b, t, 0))
		{
			if (b.rind < 0)
			{
				const int count = -1 * b.rind;
				const int start = -1 * b.lind;
				for (int i = start; i < start + count; i += 3)
					intersect_triangle(ray, V, i, &ind, &t, &u, &v);	
			}
			else
			{
				Box l, r;
				int lind = b.lind;
				if (b.lind < 2048)
				{
					l = boost[lind];
					r = boost[lind + 1];
				}
				else
				{
					l = boxes[lind];
					r = boxes[lind + 1];
				}
                float t_l = FLT_MAX;
                float t_r = FLT_MAX;
                int lhit = intersect_box(ray, l, t, &t_l);
                int rhit = intersect_box(ray, r, t, &t_r);
                if (lhit && t_l >= t_r)
                    stack[s_i++] = lind;
                if (rhit)
                    stack[s_i++] = lind + 1;
                if (lhit && t_l < t_r)
                    stack[s_i++] = lind;
			}
		}
	}

	ray.t = t;
	ray.u = u;
	ray.v = v;
	ray.hit_ind = ind;
	ray.status = ind == -1 ? DEAD : FETCH;

	rays[gid] = ray;
}