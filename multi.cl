
#define PI 3.14159265359f
#define NORMAL_SHIFT 0.0003f
#define COLLISION_ERROR 0.0001f

#define BLACK (float3)(0.0f, 0.0f, 0.0f)
#define WHITE (float3)(1.0f, 1.0f, 1.0f)
#define RED (float3)(0.8f, 0.2f, 0.2f)
#define GREEN (float3)(0.2f, 0.8f, 0.2f)
#define BLUE (float3)(0.2f, 0.2f, 0.8f)
#define GREY (float3)(0.5f, 0.5f, 0.5f)

#define LUMA (float3)(0.2126f, 0.7152f, 0.0722f)

#define SUN (float3)(0.0f, 10000.0f, 0.0f)
#define SUN_BRIGHTNESS 60000.0f
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
	float3 Ks;
	float3 Ke;

	float Ni;
	float Tr;
	float roughness;

	//here's where texture and stuff goes
	int d_index;
	int d_height;
	int d_width;

	int s_index;
	int s_height;
	int s_width;

	int e_index;
	int e_height;
	int e_width;

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
static inline int intersect_box(const float3 origin, const float3 inv_dir, Box b, float t, float *t_out)
{
	float tx0 = (b.minx - origin.x) * inv_dir.x;
	float tx1 = (b.maxx - origin.x) * inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	float ty0 = (b.miny - origin.y) * inv_dir.y;
	float ty1 = (b.maxy - origin.y) * inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);


	if ((tmin >= tymax) || (tymin >= tmax))
		return (0);

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (b.minz - origin.z) * inv_dir.z;
	float tz1 = (b.maxz - origin.z) * inv_dir.z;
	float tzmin = fmin(tz0, tz1);
	float tzmax = fmax(tz0, tz1);

	if ((tmin >= tzmax) || (tzmin >= tmax))
		return (0);

    tmin = fmax(tzmin, tmin);
	tmax = fmin(tzmax, tmax);

	if (tmin > t)
		return (0);
	if (tmin <= 0.0f && tmax <= 0.0f)
		return (0);
	if (t_out)
		*t_out = fmax(0.0f, tmin);
	return (1);
}

static inline void intersect_triangle(const float3 origin, const float3 direction, __global float3 *V, int test_i, int *best_i, float *t, float *u, float *v)
{
	float this_t, this_u, this_v;
	float3 v0 = V[test_i];
	float3 v1 = V[test_i + 1];
	float3 v2 = V[test_i + 2];

	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;

	float3 h = cross(direction, e2);
	float a = dot(h, e1);

	float f = 1.0f / a;
	float3 s = origin - v0;
	this_u = f * dot(s, h);
	if (this_u < 0.0f || this_u > 1.0f)
		return;
	float3 q = cross(s, e1);
	this_v = f * dot(direction, q);
	if (this_v < 0.0f || this_u + this_v > 1.0f)
		return;
	this_t = f * dot(e2, q);
	if (this_t < *t && this_t > 0.0f)
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

static void fetch_all_tex(const Material mat, __global uchar *tex, float3 txcrd, float3 *trans, float3 *bump, float3 *spec, float3 *diff)
{
	*trans = mat.t_height ? fetch_tex(txcrd, mat.t_index, mat.t_height, mat.t_width, tex) : BLACK;
	*bump = mat.b_height ? fetch_tex(txcrd, mat.b_index, mat.b_height, mat.b_width, tex) * 2.0f - 1.0f : UNIT_Z;
	*spec = mat.s_height ? fetch_tex(txcrd, mat.s_index, mat.s_height, mat.s_width, tex) : mat.Ks;
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
//	*N_out = sample_N;

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
	Material mat = mats[M[ray.hit_ind / 3]];
	
	fetch_NT(V, N, T, ray.direction, ray.hit_ind, ray.u, ray.v, &ray.N, &txcrd);
	fetch_all_tex(mat, tex, txcrd, &ray.trans, &ray.bump, &ray.spec, &ray.diff);
	ray.N = bump_map(TN, BTN, ray.hit_ind / 3, ray.N, ray.bump);
	ray.status = BOUNCE;

	// if (ray.trans.x < 1.0f)
	// {
	// 	ray.origin = ray.origin + ray.direction * (ray.t + NORMAL_SHIFT);
	// 	ray.bounce_count--;
	// 	ray.status = TRAVERSE;
	// }

	if (dot(mat.Ke, mat.Ke) > 0.0f)
	{
		ray.color += SUN_BRIGHTNESS * ray.mask;
		ray.status = DEAD;
	}

	rays[gid] = ray;
}

/////Bounce stuff

static float GGX_D(float a, float cost)
{
	float a2 = a * a;
	printf("hi");

	float theta = acos(cost);
	if (theta != theta)
	{
		printf("it happened\n");
		theta = 0.0f; //acos(1.0f) is nan for some reason.
	}
	float cos4 = cost * cost * cost * cost;
	float tsq = a2 + tan(theta) * tan(theta);
	tsq *= tsq;
	float denom = PI * cos4 * tsq;

	return a2 / denom;
}

static float GGX_G1(float3 v, float3 n, float3 m, float a)
{
	float theta = acos(fabs(dot(v, n))); //still not sure if this should be n or m.
	//I think it has to be n, because dot(i,m) == dot(o,m) and then they'd just say g1^2
	if (theta != theta)
		theta = 0.0f; //acos(1.0f) is nan for some reason.
	float radicand = 1.0f + a * a * tan(theta) * tan(theta);
	float ret = 2.0f / (1.0f + sqrt(radicand));
	return ret;
}

static float GGX_G(float3 i, float3 o, float3 m, float3 n, float a)
{
	//G is zero under these conditions, fail early to avoid nans
	if (dot(i, m) * dot(i, n) <= 0.0f)
		return 0.0f;
	if (dot(o, m) * dot(o,n) <= 0.0f)
		return 0.0f;

	float g1 = GGX_G1(i, n, m, a);
	float g2 = GGX_G1(o, n, m, a);
	return g1 * g2;
}

static float GGX_F(float3 i, float3 m, float n1, float n2)
{
	float num = n1 - n2;
	float denom = n1 + n2;
	float r0 = (num * num) / (denom * denom);
	float cos_term = pow(1.0f - fabs(dot(i, m)), 5.0f);
	return r0 + (1.0f - r0) * cos_term;
}

static float GGX_eval(float3 i, float3 o, float3 m, float3 n, float a, float n1, float n2)
{
	// == F * G * D / 4 |i dot n| |o dot n|

	float f = GGX_F(i, n, n1, n2);
	float g = GGX_G(i, o, m, n, a);
	float d = GGX_D(a, dot(m,n));
	//currently overriding f
	f = 1.0f;

	if (f * g * d <= 0.0f)
		return 0.0f;

	float denom = 4.0f * dot(i, n) * dot(o, n);
	float eval = (f * g * d) / denom;
	return eval;
}

static float3 GGX_NDF(float3 i, float3 n, float r1, float r2, float a)
{
	//return a direction m with relative probability GGX_D(m)|m dot n|
	float theta = atan(a * sqrt(r1) * rsqrt(1.0f - r1));
	float phi = 2.0f * PI * r2;
	
	//local orthonormal system
	float3 axis = fabs(n.x) > fabs(n.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
	float3 hem_x = cross(axis, n);
	float3 hem_y = cross(n, hem_x);

	float3 x = hem_x * native_sin(theta) * native_cos(phi);
	float3 y = hem_y * native_sin(theta) * native_sin(phi);
	float3 z = n * native_cos(theta);

	float3 m = normalize(x + y + z);
	//if (dot(m, i) * dot(n, i) < 0.0f)
	//	m = normalize(z - x - y);
	return m;
}

static float GGX_weight(float3 i, float3 o, float3 m, float3 n, float a)
{
	float num = fabs(dot(i,m)) * GGX_G(i, o, m, n, a);
	float denom = fabs(dot(i, n)) * fabs(dot(m, n));
	float weight = denom > 0.0f ? num / denom : 0.0f;
	return weight;
}

__kernel void bounce( 	__global Ray *rays,
						__global uint *seeds)
{
	int gid = get_global_id(0);
	Ray ray = rays[gid];
	if (ray.status != BOUNCE)
		return;

	// ray.status = DEAD;
	// ray.color = (1.0f + ray.N) / 2.0f;
	// rays[gid] = ray;
	// return;

	uint seed0 = seeds[2 * gid];
	uint seed1 = seeds[2 * gid + 1];
	float r1 = get_random(&seed0, &seed1);
	float r2 = get_random(&seed0, &seed1);

	float3 n = ray.N;
	float3 i = ray.direction * -1.0f;
	float a = 0.1f;
	a = (1.2f - 0.2f * sqrt(fabs(dot(i,n)))) * a; //softening approximation to reduce weight variance
	float3 m = GGX_NDF(i, n, r1, r2, a); //sampling microfacet normal

	//inside or outside the glass?
	float norm_sign = dot(n, i) > 0.0f ? 1.0f : -1.0f;  
	float ni, nt;
	if (norm_sign > 0.0f)
	{
		ni = 1.0f;
		nt = 1.5f;
	}
	else
	{
		ni = 1.5f;
		nt = 1.0f;
	}
	
	float3 o;
	float3 weight = WHITE;
	if (dot(ray.spec, ray.spec) > 0.0f) 
	{
	 	if (get_random(&seed0, &seed1) <= GGX_F(i, m, ni, nt))
	 	{
	 		o = normalize(2.0f * fabs(dot(i, m)) * m - i);
	 		weight *= GGX_weight(i, o, m, n, a);
	 	}
	 	else
	 	{
	 		float index = ni / nt;
	 		float c = dot(i,m);
	 		float coeff = index * c - norm_sign * sqrt(1.0f + index * (c * c - 1.0f));
	 		if (coeff != coeff)
	 		{
	 			//if coeff is Nan, means total internal reflection
	 			o = normalize(2.0f * fabs(dot(i, m)) * m - i);
				weight *= GGX_weight(i, o, m, n, a);
			}
	 		else
	 		{
	 			o = normalize((coeff * m) - (index * i));
	 			weight = GGX_weight(i, o, m, n, a) * norm_sign > 0.0f ? WHITE : ray.diff;
	 		}
		}
	}
	else
	{
		//Cosine-weighted pure diffuse reflection
		//local orthonormal system
		float3 axis = fabs(ray.N.x) > fabs(ray.N.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
		float3 hem_x = cross(axis, ray.N);
		float3 hem_y = cross(ray.N, hem_x);

		//generate random direction on the unit hemisphere (cosine-weighted)
		float r = native_sqrt(r1);
		float theta = 2.0f * PI * r2;

		//combine for new direction
		o = normalize(hem_x * r * native_cos(theta) + hem_y * r * native_sin(theta) + ray.N * native_sqrt(max(0.0f, 1.0f - r1)));
		weight = ray.diff;
	}

	float o_sign = dot(n, o) > 0.0f ? 1.0f : -1.0f;  
	ray.mask *= weight;
	ray.origin = ray.origin + ray.direction * ray.t + n * o_sign * NORMAL_SHIFT;
	ray.direction = o;
	ray.inv_dir = 1.0f / o;
	ray.status = TRAVERSE;
	if (dot(ray.mask, ray.mask) == 0.0f)
		ray.status = DEAD;

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
		output[ray.pixel_id] += ray.color;
		// if (ray.hit_ind == -1)
		// {
		// 	if (ray.bounce_count > 0)
		// 		output[ray.pixel_id] += ray.mask * SUN_BRIGHTNESS * fabs(pow(dot(ray.direction, UNIT_Y), 1.0f));
		// 	else
		// 		output[ray.pixel_id] += 0.1f * SUN_BRIGHTNESS;
		// }
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
		ray.hit_ind = 0;
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
		if (intersect_box(ray.origin, ray.inv_dir, b, t, 0))
		{
			if (b.rind < 0)
			{
				const int count = -1 * b.rind;
				const int start = -1 * b.lind;
				for (int i = start; i < start + count; i += 3)
					intersect_triangle(ray.origin, ray.direction, V, i, &ind, &t, &u, &v);	
			}
			else
			{
				Box l, r;
				if (b.lind < 2048)
				{
					l = boost[b.lind];
					r = boost[b.rind];
				}
				else
				{
					l = boxes[b.lind];
					r = boxes[b.rind];
				}
                float t_l = FLT_MAX;
                float t_r = FLT_MAX;
                int lhit = intersect_box(ray.origin, ray.inv_dir, l, t, &t_l);
                int rhit = intersect_box(ray.origin, ray.inv_dir, r, t, &t_r);
                if (lhit && t_l >= t_r)
                    stack[s_i++] = b.lind;
                if (rhit)
                    stack[s_i++] = b.rind;
                if (lhit && t_l < t_r)
                    stack[s_i++] = b.lind;
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
