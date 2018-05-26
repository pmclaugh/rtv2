#define PI 3.14159265359f
#define NORMAL_SHIFT 0.0003f
#define COLLISION_ERROR 0.0001f

#define BLACK (float3)(0.0f, 0.0f, 0.0f)
#define WHITE (float3)(1.0f, 1.0f, 1.0f)
#define RED (float3)(0.8f, 0.2f, 0.2f)
#define GREEN (float3)(0.2f, 0.8f, 0.2f)
#define BLUE (float3)(0.2f, 0.2f, 0.8f)
#define GREY (float3)(0.5f, 0.5f, 0.5f)

#define BRIGHTNESS 100000000.0f

#define UNIT_X (float3)(1.0f, 0.0f, 0.0f)
#define UNIT_Y (float3)(0.0f, 1.0f, 0.0f)
#define UNIT_Z (float3)(0.0f, 0.0f, 1.0f)

#define RR_PROB 0.1f


#define CAMERA_BOUNCES 3
#define LIGHT_BOUNCES 3

typedef struct s_ray {
	float3 origin;
	float3 direction;
	float3 inv_dir;

	float3 color;
	float3 mask;

	int hit_ind;
	int bounce_count;
	int status;
	int type;
}				Ray;

typedef struct s_path {
	float3 origin;
	float3 direction;
	float3 normal;
	float3 mask;
	float G;
	float pC;
	float pL;
	int hit_light;
}				Path;

typedef struct s_material
{
	float3 Kd;
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
	float3 pos;
	float3 origin;
	float3 direction;
	float3 focus;
	float3 d_x;
	float3 d_y;
	int width;
	float focal_length;
	float aperture;
}				Camera;

typedef struct s_box
{
	float min_x;
	float min_y;
	float min_z;
	int l_ind;
	float max_x;
	float max_y;
	float max_z;
	int r_ind;
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

static int intersect_box(const float3 origin, const float3 inv_dir, Box b, float t, float *t_out)
{
	float tx0 = (b.min_x - origin.x) * inv_dir.x;
	float tx1 = (b.max_x - origin.x) * inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	float ty0 = (b.min_y - origin.y) * inv_dir.y;
	float ty1 = (b.max_y - origin.y) * inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);


	if ((tmin >= tymax) || (tymin >= tmax))
		return (0);

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (b.min_z - origin.z) * inv_dir.z;
	float tz1 = (b.max_z - origin.z) * inv_dir.z;
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

static int intersect_triangle(const float3 origin, const float3 direction, __global float3 *V, int test_i, int *best_i, float *t, float *u, float *v)
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
		return 0;
	float3 q = cross(s, e1);
	this_v = f * dot(direction, q);
	if (this_v < 0.0f || this_u + this_v > 1.0f)
		return 0;
	this_t = f * dot(e2, q);
	if (this_t < *t && this_t > 0.0f)
	{
		*t = this_t;
		*u = this_u;
		*v = this_v;
		*best_i = test_i;
		return 1;
	}
	return 0;
}

static void orthonormal(float3 z, float3 *x, float3 *y)
{
	//local orthonormal system
	float3 axis = fabs(z.x) > fabs(z.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
	axis = cross(axis, z);
	*x = axis;
	*y = cross(z, axis);
}

static float3 direction_uniform_hemisphere(float3 x, float3 y, float u1, float theta, float3 normal)
{
	float r = native_sqrt(1.0f - u1 * u1);
	return normalize(x * r * native_cos(theta) + y * r * native_sin(theta) + normal * u1);
}

static float3 direction_cos_hemisphere(float3 x, float3 y, float u1, float theta, float3 normal)
{
	float r = native_sqrt(u1);
	return normalize(x * r * native_cos(theta) + y * r * native_sin(theta) + normal * native_sqrt(max(0.0f, 1.0f - u1)));
}

static float3 diffuse_BDRF_sample(float3 normal, int way, uint *seed0, uint *seed1)
{
	float3 x, y;
	orthonormal(normal, &x, &y);

	float u1 = get_random(seed0, seed1);
	float u2 = get_random(seed0, seed1);
	float theta = 2.0f * PI * u2;
	// if (way)
		return direction_uniform_hemisphere(x, y, u1, theta, normal);
	// else
	// 	return direction_cos_hemisphere(x, y, u1, theta, normal);
}


__kernel void init_paths(const Camera cam,
						__global uint *seeds,
						__global Path *paths,
						__global int *path_lengths,
						__global int3 *lights,
						const uint light_poly_count,
						__global float3 *V,
						__global float3 *N,
						__global float3 *output,
						__global int *M,
						__global Material *mats
						)
{
	int index = get_global_id(0);
	uint seed0 = seeds[2 * index];
	uint seed1 = seeds[2 * index + 1];

	float3 origin, direction, mask, normal;
	float G = 1.0f;
	float pC, pL;
	int way = index % 2;
	if (way)
	{
		//pick a random light triangle
		int light_ind = (int)floor((float)light_poly_count * get_random(&seed0, &seed1));
		light_ind %= light_poly_count;
		int3 light = lights[light_ind];
		
		float3 v0, v1, v2;
		v0 = V[light.x];
		v1 = V[light.y];
		v2 = V[light.z];
		normal = normalize(N[light.x]);
		direction = diffuse_BDRF_sample(normal, way, &seed0, &seed1);

		float3 Ke = mats[M[light.x / 3]].Ke;
		mask = Ke * BRIGHTNESS * dot(direction, normal);		

		//pick random point just off of that triangle
		float r1 = get_random(&seed0, &seed1);
		float r2 = get_random(&seed0, &seed1);
		origin = (1.0f - sqrt(r1)) * v0 + (sqrt(r1) * (1.0f - r2)) * v1 + (r2 * sqrt(r1)) * v2 + NORMAL_SHIFT * normal;

		pC = 1.0f;
		pL = 1.0f;
	}
	else
	{
		//stratified sampling on the camera plane
		float x = (float)((index / 2) % cam.width) + get_random(&seed0, &seed1);
		float y = (float)((index / 2) / cam.width) + get_random(&seed0, &seed1);
		origin = cam.focus;

		float3 through = cam.origin + cam.d_x * x + cam.d_y * y;
		direction = normalize(cam.focus - through);

		float3 aperture;
		aperture.x = (get_random(&seed0, &seed1) - 0.5f) * cam.aperture;
		aperture.y = (get_random(&seed0, &seed1) - 0.5f) * cam.aperture;
		aperture.z = (get_random(&seed0, &seed1) - 0.5f) * cam.aperture;

		float3 f_point = cam.focus + cam.focal_length * direction;

		origin = cam.focus + aperture;
		direction = normalize(f_point - origin);
		normal = direction;
		mask = WHITE;

		pC = 1.0f;
		pL = 1.0f;

	}

	paths[index].origin = origin;
	paths[index].direction = direction;
	paths[index].mask = mask;
	paths[index].normal = normal;
	paths[index].G = G;
	paths[index].hit_light = 0;
	path_lengths[index] = 0;

	seeds[2 * index] = seed0;
	seeds[2 * index + 1] = seed1;
}


static inline int inside_box(float3 point, Box box)
{
	if (box.min_x <= point.x && point.x <= box.max_x)
		if (box.min_y <= point.y && point.y <= box.max_y)
			if (box.min_z <= point.z && point.z <= box.max_z)
				return 1;
	return 0;
}

static inline int visibility_test(float3 origin, float3 direction, float t, __global Box *boxes, __global float3 *V)
{
	int stack[32];
	stack[0] = 0;
	int s_i = 1;

	float u, v;
	int ind = -1;

	float3 inv_dir = 1.0f / direction;

	while (s_i)
	{
		int b_i = stack[--s_i];
		Box box = boxes[b_i];

		float this_t;
		int result = intersect_box(origin, inv_dir, box, t, &this_t);
		if (result)
		{
			if (box.r_ind < 0)
			{
				const int count = -1 * box.r_ind;
				const int start = -1 * box.l_ind;
				for (int i = start; i < start + count; i += 3)
					if (intersect_triangle(origin, direction, V, i, &ind, &t, &u, &v))
						break ;
			}
			else
			{
				Box l, r;
				l = boxes[box.l_ind];
				r = boxes[box.r_ind];
                float t_l = FLT_MAX;
                float t_r = FLT_MAX;
                int lhit = intersect_box(origin, inv_dir, l, t, &t_l);
                int rhit = intersect_box(origin, inv_dir, r, t, &t_r);

                if (lhit && t_l >= t_r)
                    stack[s_i++] = box.l_ind;
                if (rhit && t_r < t)
                    stack[s_i++] = box.r_ind;
                if (lhit && t_l < t_r)
                    stack[s_i++] = box.l_ind;
			}
		}
	}

	if (ind == -1)
		return 1;
	return 0;
}

static float geometry_term(Path a, Path b)
{
	float3 origin, direction, distance;
	float t;

	origin = a.origin;
	distance = b.origin - origin;
	t = native_sqrt(dot(distance, distance));
	direction = normalize(distance);

	float camera_cos = max(0.0f, dot(a.normal, direction));
	float light_cos = max(0.0f, dot(b.normal, -1.0f * direction));

	return (camera_cos * light_cos) / (t * t);
}

#define CAMERA_VERTEX(x) (paths[2 * index + row_size * (x)])
#define LIGHT_VERTEX(x) (paths[(2 * index + 1) + row_size * (x)])

#define GEOM(x) x < s ? (LIGHT_VERTEX(x).G) : (x == s ? this_geom : (CAMERA_VERTEX(s + t - (x)).G))
#define PC(x) x < s ? (LIGHT_VERTEX(x).pC) : x == s ? this_pC : (CAMERA_VERTEX(s + t - x).pC)
#define PL(x) x < s ? (LIGHT_VERTEX(x).pL) : x == s ? this_pL : (CAMERA_VERTEX(s + t - x).pL)

__kernel void connect_paths(__global Path *paths,
							__global int *path_lengths,
							__global Box *boxes,
							__global float3 *V,
							__global float3 *output)
{
	//uses half global size
	int index = get_global_id(0);
	int row_size = 2 * get_global_size(0);

	int camera_length = path_lengths[2 * index];
	int light_length = path_lengths[2 * index + 1];

	float3 sum = BLACK;
	int count = 0;

	for (int t = 2; t <= camera_length + 1; t++)
	{
		Path camera_vertex = CAMERA_VERTEX(t - 1);
		for (int s = 0; s <= light_length; s++)
		{
			if (s == 0)
			{
				if (camera_vertex.hit_light)
				{
					float weight = 0.0f;
					for (int k = 0; k < t; k++)
						weight += 1.0f / (CAMERA_VERTEX(k).G);
					float ratio = t;
					sum += camera_vertex.mask / (ratio * weight);
					break ;
				}
				continue ;
			}
			count++;
			Path light_vertex = LIGHT_VERTEX(s - 1);

			float3 origin, direction, distance;
			origin = camera_vertex.origin;
			distance = light_vertex.origin - origin;
			float d = native_sqrt(dot(distance, distance));
			direction = normalize(distance);

			float camera_cos = max(0.0f, dot(camera_vertex.normal, direction));
			float light_cos = max(0.0f, dot(light_vertex.normal, -1.0f * direction));

			if (camera_cos * light_cos <= 0.0f)
				continue ;
			if (!visibility_test(origin, direction, d, boxes, V))
				continue ;
			if (dot(light_vertex.mask, light_vertex.mask) == 0.0f)
				break ;
			
			float this_geom = geometry_term(camera_vertex, light_vertex);
			float3 contrib = light_vertex.mask * camera_vertex.mask * camera_cos * this_geom;
			if (s == 1)
				contrib *= light_cos;

			float p[16];
			for (int k = 0; k < s + t + 1; k++)
				p[k] = (GEOM(s)) / (GEOM(k));

			float weight = 0.0f;
			for (int k = 0; k < s + t + 1; k++)
				weight += p[k];

			float ratio = (float)(s + t + 1);

			sum += contrib / (ratio * weight);
		}
	}
	output[index] += sum;
}

static void surface_vectors(__global float3 *V, __global float3 *N, __global float3 *T, float3 dir, int ind, float u, float v, float3 *N_out, float3 *true_N_out, float3 *txcrd_out)
{
	float3 v0 = V[ind];
	float3 v1 = V[ind + 1];
	float3 v2 = V[ind + 2];
	float3 geom_N = normalize(cross(v1 - v0, v2 - v0));
	*true_N_out = geom_N;

	v0 = N[ind];
	v1 = N[ind + 1];
	v2 = N[ind + 2];
	float3 sample_N = normalize((1.0f - u - v) * v0 + u * v1 + v * v2);

	*N_out = sample_N;

	float3 txcrd = (1.0f - u - v) * T[ind] + u * T[ind + 1] + v * T[ind + 2];
	txcrd.x -= floor(txcrd.x);
	txcrd.y -= floor(txcrd.y);
	if (txcrd.x < 0.0f)
		txcrd.x = 1.0f + txcrd.x;
	if (txcrd.y < 0.0f)
		txcrd.y = 1.0f + txcrd.y;
	*txcrd_out = txcrd;	
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

static void fetch_all_tex(__global int *M, __global Material *mats, __global uchar *tex, int ind, float3 txcrd, float3 *diff, float3 *spec, float3 *bump, float3 *trans, float3 *Ke)
{
	const Material mat = mats[M[ind / 3]];
	*trans = mat.t_height ? fetch_tex(txcrd, mat.t_index, mat.t_height, mat.t_width, tex) : BLACK;
	*bump = mat.b_height ? fetch_tex(txcrd, mat.b_index, mat.b_height, mat.b_width, tex) * 2.0f - 1.0f : UNIT_Z;
	*spec = mat.s_height ? fetch_tex(txcrd, mat.s_index, mat.s_height, mat.s_width, tex) : mat.Ks;
	*diff = mat.d_height ? fetch_tex(txcrd, mat.d_index, mat.d_height, mat.d_width, tex) : mat.Kd;
	*Ke = mat.Ke;
}

static float3 bump_map(__global float3 *TN, __global float3 *BTN, int ind, float3 sample_N, float3 bump)
{
	float3 tangent = TN[ind];
	float3 bitangent = BTN[ind];
	tangent = normalize(tangent - sample_N * dot(sample_N, tangent));
	return normalize(tangent * bump.x + bitangent * bump.y + sample_N * bump.z);
}

__kernel void trace_paths(__global Path *paths,
						__global float3 *V,
						__global float3 *T,
						__global float3 *N,
						__global float3 *TN,
						__global float3 *BTN,
						__global int *M,
						__global Material *mats,
						__global uchar *tex,
						__global int *path_lengths,
						__global Box *boxes,
						__global uint *seeds,
						__global float3 *output)
{
	int index = get_global_id(0);
	int row_size = get_global_size(0);
	
	uint seed0, seed1;
	seed0 = seeds[2 * index];
	seed1 = seeds[2 * index + 1];

	int stack[32];
	float3 origin, direction, inv_dir, mask, old_dir;

	//fetch initial data
	origin = paths[index].origin;
	direction = paths[index].direction;
	mask = paths[index].mask;
	path_lengths[index] = 1;
	old_dir = direction;
	
	int way = index % 2;
	int limit = way ? LIGHT_BOUNCES : CAMERA_BOUNCES;

	int length = 1;
	for (length = 1; length <= limit; length++)
	{
		inv_dir = 1.0f / direction;
		//reset stack and results
		stack[0] = 0;
		int s_i = 1;
		float t, u, v;
		t = FLT_MAX;
		int ind = -1;

		//find collision point
		while (s_i)
		{
			int b_i = stack[--s_i];
			Box box = boxes[b_i];

			//check
			if (intersect_box(origin, inv_dir, box, t, 0))
			{
				if (box.r_ind < 0)
				{
					const int count = -1 * box.r_ind;
					const int start = -1 * box.l_ind;
					for (int i = start; i < start + count; i += 3)
						intersect_triangle(origin, direction, V, i, &ind, &t, &u, &v);	
				}
				else
				{
					Box l, r;
					l = boxes[box.l_ind];
					r = boxes[box.r_ind];
	                float t_l = FLT_MAX;
	                float t_r = FLT_MAX;
	                int lhit = intersect_box(origin, inv_dir, l, t, &t_l);
	                int rhit = intersect_box(origin, inv_dir, r, t, &t_r);
	                if (lhit && t_l >= t_r)
	                    stack[s_i++] = box.l_ind;
	                if (rhit)
	                    stack[s_i++] = box.r_ind;
	                if (lhit && t_l < t_r)
	                    stack[s_i++] = box.l_ind;
				}
			}
		}

		//check for miss
		if (ind == -1)
			break ;

		//get normal and texture coordinate
		float3 normal, true_normal, txcrd;
		surface_vectors(V, N, T, direction, ind, u, v, &normal, &true_normal, &txcrd);

		//get all texture-like values
		float3 diff, spec, bump, trans, Ke;
		fetch_all_tex(M, mats, tex, ind, txcrd, &diff, &spec, &bump, &trans, &Ke);

		// did we hit a light?
		if (dot(Ke, Ke) > 0.0f)
		{
			paths[index + row_size * length].origin = origin + direction * t + true_normal * NORMAL_SHIFT;
			paths[index + row_size * length].mask = mask * Ke * BRIGHTNESS;
			paths[index + row_size * length].normal = normal;
			paths[index + row_size * length].G = geometry_term(paths[index + length * row_size], paths[index + (length - 1) * row_size]);
			paths[index + row_size * length].pC = 1 / (2.0f * PI);
			paths[index + row_size * length].pL = -1.0f;
			paths[index + row_size * length].hit_light = 1;
			break ;
		}

		// bump map (malfunctioning)
		normal = bump_map(TN, BTN, ind / 3, normal, bump);

		//color
		mask *= diff;
		if (way)
			mask *= max(0.0f, dot(-1.0f * direction, normal));

		//sample and evaluate BRDF
		float3 out = diffuse_BDRF_sample(normal, way, &seed0, &seed1);

		float pC, pL;
		pC = 1 / (2.0f * PI);
		pL = 1 / (2.0f * PI);

		//update stuff
		origin = origin + direction * t + true_normal * NORMAL_SHIFT;
		direction = out;
		paths[index + row_size * length].origin = origin;
		paths[index + row_size * length].direction = direction;
		paths[index + row_size * length].mask = mask;
		paths[index + row_size * length].normal = normal;
		paths[index + row_size * length].G = geometry_term(paths[index + length * row_size], paths[index + (length - 1) * row_size]);
		paths[index + row_size * length].pC = pC;
		paths[index + row_size * length].pL = pL;
		paths[index + row_size * length].hit_light = 0;

		if (!way)
			mask *= max(0.0f, dot(out, normal));

	}
	path_lengths[index] = length;
	seeds[2 * index] = seed0;
	seeds[2 * index + 1] = seed1;
}