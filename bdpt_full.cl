#define PI 3.14159265359f
#define NORMAL_SHIFT 0.0003f
#define COLLISION_ERROR 0.0001f

#define BLACK (float3)(0.0f, 0.0f, 0.0f)
#define WHITE (float3)(1.0f, 1.0f, 1.0f)
#define RED (float3)(0.8f, 0.2f, 0.2f)
#define GREEN (float3)(0.2f, 0.8f, 0.2f)
#define BLUE (float3)(0.2f, 0.2f, 0.8f)
#define GREY (float3)(0.5f, 0.5f, 0.5f)

#define BRIGHTNESS 1.0f //current state of tonemapping makes brightness value irrelevant

#define UNIT_X (float3)(1.0f, 0.0f, 0.0f)
#define UNIT_Y (float3)(0.0f, 1.0f, 0.0f)
#define UNIT_Z (float3)(0.0f, 0.0f, 1.0f)

#define RR_PROB 0.5f
#define RR_THRESHOLD 3

#define CAMERA_LENGTH 5
#define LIGHT_LENGTH 5

#define SPECULAR 20.0f

typedef struct s_path {
	float3 origin;
	float3 direction;
	float3 normal;
	float3 mask;
	float G;
	float pC;
	float pL;
	int hit_light;
	int specular;
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

	if (a < 0.0001f)
		return 0;

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

static float3 direction_uniform_hemisphere(float3 x, float3 y, float u1, float phi, float3 normal)
{
	float r = native_sqrt(1.0f - u1 * u1);
	return normalize(x * r * native_cos(phi) + y * r * native_sin(phi) + normal * u1);
}

static float3 direction_cos_hemisphere(float3 x, float3 y, float u1, float phi, float3 normal)
{
	float r = native_sqrt(u1);
	return normalize(x * r * native_cos(phi) + y * r * native_sin(phi) + normal * native_sqrt(max(0.0f, 1.0f - u1)));
}

static float3 diffuse_BRDF_sample(float3 normal, int way, uint *seed0, uint *seed1)
{
	float3 x, y;
	orthonormal(normal, &x, &y);

	float u1 = get_random(seed0, seed1);
	float u2 = get_random(seed0, seed1);
	float phi = 2.0f * PI * u2;
	if (way)
		return direction_uniform_hemisphere(x, y, u1, phi, normal);
	else
		return direction_cos_hemisphere(x, y, u1, phi, normal);
}

static float3 gloss_BRDF_sample(float3 normal, float3 spec_dir, float3 in, float exponent, uint *seed0, uint *seed1)
{
	//pure specular direction
	float3 x, y;
	orthonormal(spec_dir, &x, &y);
	float u1 = get_random(seed0, seed1);
	float u2 = get_random(seed0, seed1);
	float phi = 2.0f * PI * u2;

	u1 = pow(u1, 1.0f / (exponent + 1.0f));
	float3 out = normalize(x * (native_sqrt(1.0f - u1 * u1) * native_cos(phi)) + y * (native_sqrt(1.0f - u1 * u1)) * native_sin(phi) + u1 * spec_dir);
	if (dot(out, normal) <= 0.0f)
		out = normalize(u1 * spec_dir - x * (native_sqrt(1.0f - u1 * u1) * native_cos(phi)) - y * (native_sqrt(1.0f - u1 * u1)) * native_sin(phi));
	return out;
}

static float surface_area(int3 triangle, __global float3 *V)
{
	const float3 e1 = V[triangle.y] - V[triangle.x];
	const float3 e2 = V[triangle.z] - V[triangle.x];
	float angle = acos(dot(normalize(e1), normalize(e2)));
	return (native_sqrt(dot(e1, e1)) * native_sqrt(dot(e2, e2)) / 2.0f) * native_sin(angle);
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
		direction = diffuse_BRDF_sample(normal, way, &seed0, &seed1);

		float3 Ke = mats[M[light.x / 3]].Ke;
		mask = Ke * BRIGHTNESS;	

		//pick random point just off of that triangle
		float r1 = get_random(&seed0, &seed1);
		float r2 = get_random(&seed0, &seed1);
		origin = (1.0f - sqrt(r1)) * v0 + (sqrt(r1) * (1.0f - r2)) * v1 + (r2 * sqrt(r1)) * v2 + NORMAL_SHIFT * normal;

		pC = 1.0f / (2.0f * PI);
		pL = 1.0f / ((float)light_poly_count * surface_area(light, V));
		mask /= pL;
	}
	else
	{
		//stratified sampling on the camera plane
		float x = (float)((index / 2) % cam.width);
		float y = (float)((index / 2) / cam.width);
		origin = cam.focus;

		float3 through = cam.origin + cam.d_x * (x + 0.98f * get_random(&seed0, &seed1)) + cam.d_y * (y + 0.98f * get_random(&seed0, &seed1));
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

		pC = 1.0f / (float)(cam.width * cam.width);
		pL = 1.0f / (2.0f * PI);
		mask /= pC;
	}

	paths[index].origin = origin;
	paths[index].direction = direction;
	paths[index].mask = mask;
	paths[index].normal = normal;
	paths[index].G = G;
	paths[index].hit_light = 0;
	paths[index].pC = pC;
	paths[index].pL = pL;
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

	float camera_cos = dot(a.normal, direction);
	float light_cos = dot(b.normal, -1.0f * direction);

	return fabs(camera_cos * light_cos) / (t * t);
}

static float pdf(float3 in, Path p, float3 out, int way)
{
	//in and out should both point away from p.
	if (p.specular)
	{
		float3 spec_dir = 2.0f * dot(in, p.normal) * p.normal - in;
		return (SPECULAR + 2.0f) * pow(max(0.0f, dot(out, spec_dir)), SPECULAR) / (2.0f * PI);
	}
	else
		if (!way) //NB "way" seems flipped here but that's the point
			return 1.0f / (2.0f * PI);
		else
			return max(0.0f, dot(p.normal, out)) / PI;
}

static float BRDF(float3 in, const Path p, float3 out)
{
	//in and out should both point away from p.
	if (p.specular)
	{
		float3 spec_dir = 2.0f * dot(in, p.normal) * p.normal - in;
		float ret = (SPECULAR + 2.0f) * pow(max(0.0f, dot(out, spec_dir)), SPECULAR) / (2.0f * PI);
		if (ret > 0.0f && ret == ret)
			return ret;
		else
			return 0.0f;
	}
	else
		return max(0.0f, dot(p.normal, out)) / PI;
}

#define CAMERA_VERTEX(x) (paths[2 * index + row_size * (x)])
#define LIGHT_VERTEX(x) (paths[(2 * index + 1 + sample * jump) % row_size + row_size * (x)])

#define GEOM(x) ((x) < s ? (LIGHT_VERTEX(x).G) : ((x) == s ? this_geom : (CAMERA_VERTEX(s + t - (x)).G)))

#define PL(x) ((x) == t - 2 ? prev_pL : (x) < s ? (LIGHT_VERTEX(x).pL) : (x) == s ? this_pL : (CAMERA_VERTEX(s + t - (x)).pL))

#define PC(x) ((x) == s - 2 ? prev_pC : (x) < s - 1 ? (LIGHT_VERTEX(x).pC) : (x) == s - 1 ? this_pC : (CAMERA_VERTEX(s + t - (x) - 1).pC))

#define QC(x) (s + t - (x) < RR_THRESHOLD ? 1.0f : 1.0f - RR_PROB)
#define QL(x) ((x) < RR_THRESHOLD ? 1.0f : 1.0f - RR_PROB)

#define RESAMPLE_COUNT 1

__kernel void connect_paths(__global Path *paths,
							__global int *path_lengths,
							__global Box *boxes,
							__global float3 *V,
							__global float3 *output)
{
	//uses half global size
	int index = get_global_id(0);
	int row_size = 2 * get_global_size(0);

	float3 sum = BLACK;

	int jump = row_size / RESAMPLE_COUNT;

	int camera_length = path_lengths[2 * index];
	float p[32];

	for (int sample = 0; sample < RESAMPLE_COUNT; sample++)
	{
		int light_length = path_lengths[(2 * index + 1 + sample * jump) % row_size];
		for (int t = 2; t <= camera_length; t++)
		{
			Path camera_vertex = CAMERA_VERTEX(t - 1);

			// //handle s==0
			int s = 0;
			if (camera_vertex.hit_light)
			{
				//causes fireflies with specular objects. needs some sort of fix.
				float3 contrib = camera_vertex.mask * BRIGHTNESS;
				//initialize with ratios
				for (int k = 0; k < t; k++)
				{
					if (k == 0)
						p[k] = (LIGHT_VERTEX(0).pL) / (camera_vertex.pC * camera_vertex.G);
					else
						p[k] = (QL(k) * CAMERA_VERTEX(t - k - 1).pL * CAMERA_VERTEX(t - k).G) / (QC(k) * CAMERA_VERTEX(t - k - 1).pC * CAMERA_VERTEX(t - k - 1).G);
				}
				//multiply through
				for (int k = 0; k < t; k++)
					p[k + 1] = p[k + 1] * p[k];

				//append 1.0f
				p[t] = 1.0f;

				float weight = 0.0f;
				for (int k = 0; k < t + 1; k++)
					weight += p[k];

				float ratio = (float)(t + 1);

				float test = 1.0f / (ratio * weight);

				if (test == test && weight == weight)
					sum += contrib * test;
				continue;
			}
			for (s = 1; s <= light_length; s++)
			{
				Path light_vertex = LIGHT_VERTEX(s - 1);

				float3 direction, distance;
				distance = light_vertex.origin - camera_vertex.origin;
				float d = native_sqrt(dot(distance, distance));
				direction = normalize(distance);

				float camera_cos = dot(camera_vertex.normal, direction);
				float light_cos = dot(light_vertex.normal, -1.0f * direction);

				if (camera_cos <= 0.0f || light_cos <= 0.0f)
					continue ;
				if (!visibility_test(camera_vertex.origin, direction, d, boxes, V))
					continue ;

				float this_geom = geometry_term(camera_vertex, light_vertex);
				float this_pL, this_pC, BRDF_L, BRDF_C, prev_pL, prev_pC;
				float3 light_in, camera_in;

				//the parts based on light_in
				if (s == 1)
				{
					prev_pC = 1.0f; //placeholder, won't be accessed
					this_pL = 1.0f / (2.0f * PI);
				}
				else
				{
					light_in = normalize(LIGHT_VERTEX(s - 2).origin - light_vertex.origin);
					this_pL = pdf(light_in, light_vertex, -1.0f * direction, 0);
					prev_pC = pdf(-1.0f * direction, light_vertex, light_in, 1); //these 0s and 1s suck and should be fixed
				}

				//the parts based on camera_in
				if (t == 1)
				{
					prev_pL = 1.0f; //placeholder. won't be accessed
					this_pC = 1.0f;
				}
				else
				{
					camera_in = normalize(CAMERA_VERTEX(t - 2).origin - camera_vertex.origin);
					this_pC = pdf(camera_in, camera_vertex, direction, 1);
					prev_pL = pdf(direction, camera_vertex, camera_in, 0);
				}

				//evaluate BRDF for both
				BRDF_C = BRDF(camera_in, camera_vertex, direction);
				BRDF_L = s == 1 ? 1.0f : BRDF(light_in, light_vertex, -1.0f * direction);

				float3 contrib = light_vertex.mask * camera_vertex.mask * BRDF_L * BRDF_C * this_geom;
				
				//initialize with ratios
				for (int k = 0; k < s + t; k++)
					p[k] = (QL(k) * GEOM(k) * PL(k)) / (QC(k) * GEOM(k + 1) * PC(k));

				//multiply through
				for (int k = 0; k < s + t; k++)
					p[k + 1] = p[k + 1] * p[k];

				//pick pivot and append a 1.0f
				float pivot = p[s - 1];
				p[s + t] = 1.0f;
				for (int k = 0; k < s + t + 1; k++)
					p[k] /= pivot;

				float weight = 0.0f;
				for (int k = 0; k < s + t + 1; k++)
					weight += p[k];

				float ratio = (float)(s + t + 1);

				float test = 1.0f / (ratio * weight);

				if (test == test)
					sum += contrib * test;
			}
		}
	}
	output[index] = sum / (float)RESAMPLE_COUNT;
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


#define LENGTH length

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
	old_dir = direction;
	
	int way = index % 2;
	int limit = way ? LIGHT_LENGTH : CAMERA_LENGTH;

	int length;

	float pC = 1.0f;
	float pL = 1.0f / (2.0f * PI);

	int hit_light = 0;

	for (length = 1; length < limit && !hit_light; length++)
	{	
		//reset stack and results
		stack[0] = 0;
		int s_i = 1;
		float t, u, v;
		t = FLT_MAX;
		int ind = -1;

		inv_dir = 1.0f / direction;
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

		//update position
		origin = origin + direction * t + true_normal * NORMAL_SHIFT;

		//get all texture-like values
		float3 diff, spec, bump, trans, Ke;
		fetch_all_tex(M, mats, tex, ind, txcrd, &diff, &spec, &bump, &trans, &Ke);

		// bump map
		normal = bump_map(TN, BTN, ind / 3, normal, bump);

		//direction
		float3 out;
		int specular = dot(spec, spec) > 0.0f ? 1 : 0;
		float3 spec_dir;
		if (dot(Ke, Ke) > 0.0f)
		{
			if (way)
				break;
			mask *= Ke;
			hit_light = 1;
			paths[index + row_size * (LENGTH - 1)].pL = 1.0f / (2.0f * PI);
		}
		else if (specular)
		{
			mask *= spec;
			spec_dir = 2.0f * dot(-1.0f * direction, normal) * normal + direction;
			out = gloss_BRDF_sample(normal, spec_dir, direction, SPECULAR, &seed0, &seed1);
		}
		else
		{
			mask *= diff;
			if (way)
				mask *= max(0.0f, dot(-1.0f * direction, normal));
			out = diffuse_BRDF_sample(normal, way, &seed0, &seed1);
		}


		//write vertex
		paths[index + row_size * LENGTH].origin = origin;
		paths[index + row_size * LENGTH].mask = mask;
		paths[index + row_size * LENGTH].normal = normal;
		paths[index + row_size * LENGTH].G = geometry_term(paths[index + LENGTH * row_size], paths[index + (LENGTH - 1) * row_size]);
		paths[index + row_size * LENGTH].pC = pC;
		paths[index + row_size * LENGTH].pL = pL;
		paths[index + row_size * LENGTH].hit_light = hit_light;
		paths[index + row_size * LENGTH].specular = specular;

		//update back-path probability
		if (way)
			paths[index + row_size * (LENGTH - 1)].pC = pdf(out, paths[index + row_size * LENGTH], -1.0f * direction, way);
		else
			paths[index + row_size * (LENGTH - 1)].pL = pdf(out, paths[index + row_size * LENGTH], -1.0f * direction, way);

		//new probability values (to do: clean these up with pdf function)
		if (way)
		{
			if (specular)
				pL = (SPECULAR + 2.0f) * pow(dot(out, spec_dir), SPECULAR) / (2.0f * PI);
			else
			{
				mask *= 2.0f; //this might not be necessary idk
				pL = 1.0f / (2.0f * PI);
			}
		}
		else
			if (specular)
				pC = (SPECULAR + 2.0f) * pow(dot(out, spec_dir), SPECULAR) / (2.0f * PI);
			else
				pC = dot(out, normal) / (PI);

		//russian roulette
		float p = length <= RR_THRESHOLD ? 1.0f : 1.0f - RR_PROB;
		if (get_random(&seed0, &seed1) < p)
			mask /= p;
		else
		{
			length++;
			break;
		}
		direction = out;
	}

	path_lengths[index] = length;
	seeds[2 * index] = seed0;
	seeds[2 * index + 1] = seed1;
}