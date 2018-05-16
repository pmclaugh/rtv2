#include "btpt_cl.h"

#define BOUNCE_LIMIT 3;

static int intersect_box(const float3 origin, const float3 inv_dir, Box b, float t, float *t_out)
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

static void intersect_triangle(const float3 origin, const float3 direction, __global float3 *V, int test_i, int *best_i, float *t, float *u, float *v)
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

static void surface_vectors(__global float3 *V, __global float3 *N, __global float3 *T, float3 dir, int ind, float u, float v, float3 *N_out, float3 *txcrd_out)
{
	float3 v0 = V[ind];
	float3 v1 = V[ind + 1];
	float3 v2 = V[ind + 2];
	float3 geom_N = normalize(cross(v1 - v0, v2 - v0));

	v0 = N[ind];
	v1 = N[ind + 1];
	v2 = N[ind + 2];
	float3 sample_N = normalize((1.0f - u - v) * v0 + u * v1 + v * v2);

	*N_out = copysign(sample_N, dot(geom_N, sample_N));

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
						__global int *path_lengths;
						__global Box *boxes,
						__global float3 *V,
						__global float3 *T,
						__global float3 *N,
						__global float3 *TN,
						__global float3 *BTN,
						__global int *M
						__global Material *mats,
						__global uchar *tex,
						__global uint *seeds)
{
	int index = get_global_id(0);
	int row_size = get_global_size(0);

	uint seed0, seed1;
	seed0 = seeds[index];
	seed1 = seeds[index + 1];

	int stack[32];
	float3 origin, direction, inv_dir, mask;

	//fetch initial data
	origin = paths[index].origin;
	direction = paths[index].direction;
	mask = paths[index].mask;
	inv_dir = 1.0f  / direction;
	int length;
	for (length = 1; length <= BOUNCE_LIMIT; length++)
	{
		//reset stack and results
		stack[0] = 0;
		int s_i = 1;
		float t, u, v;
		t = CL_FLT_MAX;
		int ind = -1;

		//find collision point
		while (s_i)
		{
			int b_i = stack[--s_i];
			Box b = boxes[b_i];

			//check
			if (intersect_box(origin, inv_dir, b, t, 0))
			{
				if (b.rind < 0)
				{
					const int count = -1 * b.rind;
					const int start = -1 * b.lind;
					for (int i = start; i < start + count; i += 3)
						intersect_triangle(origin, direction, V, i, &ind, &t, &u, &v);	
				}
				else
				{
					Box l, r;
					l = boxes[b.lind];
					r = boxes[b.rind];
	                float t_l = FLT_MAX;
	                float t_r = FLT_MAX;
	                int lhit = intersect_box(origin, inv_dir, l, t, &t_l);
	                int rhit = intersect_box(origin, inv_dir, r, t, &t_r);
	                if (lhit && t_l >= t_r)
	                    stack[s_i++] = b.lind;
	                if (rhit)
	                    stack[s_i++] = b.rind;
	                if (lhit && t_l < t_r)
	                    stack[s_i++] = b.lind;
				}
			}
		}

		//check for miss
		if (ind == -1)
			break ;
		
		//get normal and texture coordinate
		float3 normal, txcrd;
		surface_vectors(V, N, T, direction, ind, u, v, &normal, &txcrd);

		//get all texture-like values
		float3 diff, spec, bump, trans, Ke;
		texture_fill(M, mats, tex, ind, txcrd, &diff, &spec, &bump, &trans, &Ke);

		//did we hit a light?
		if (dot(Ke, Ke) == 0.0f)
			break ;

		//bump map
		normal = bump_map(TN, BTN, ind, normal, bump);

		//color
		mask *= diff;

		//sample and evaluate BRDF
		int way = index % 2;
		float3 out = diffuse_BDRF_sample(normal, way);
		if (way = 1)
			mask *= dot(-1.0f * direction, normal);

		//update stuff
		paths[index + row_size * i].origin = origin + direction * t + normal * NORMAL_SHIFT;
		paths[index + row_size * i].direction = direction;
		paths[index + row_size * i].mask = mask;
		paths[index + row_size * i].normal = normal;
		
		//loop
	}
	path_lengths[index] = length;
	seeds[index] = seed0;
	seeds[index + 1] = seed1;
}