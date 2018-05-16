typedef struct s_camera
{
	float3 pos;
	float3 origin;
	float3 focus;
	float3 d_x;
	float3 d_y;
	float3 direction;
	int width;
	float focal_length;
	float aperture;
}				Camera;


__kernel void init_paths(const Camera cam,
						__global uint *seeds,
						__global Path *paths,
						__global int *path_lengths,
						__global int3 *lights,
						const int light_poly_count,
						__global float3 *V,
						)
{
	int index = get_global_id(0);
	int row_size = get_global_size(0);
	uint seed0 = seeds[index];
	uint seed1 = seeds[index + 1];

	float3 origin, direction, mask;
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
		direction = diffuse_BDRF_sample(normal, way);
		mask = WHITE * dot(direction, normal);

		//pick random point just off of that triangle
		float r1 = get_random(&ray.seed0, &ray.seed1);
		float r2 = get_random(&ray.seed0, &ray.seed1);
		origin = (1.0f - sqrt(r1)) * v0 + (sqrt(r1) * (1.0f - r2)) * v1 + (r2 * sqrt(r1)) * v2 + NORMAL_SHIFT * normal;
	}
	else
	{
		//stratified sampling on the camera plane
		float x = (float)(index % cam.width) + get_random(&seed0, &seed1);
		float y = (float)(index / cam.width) + get_random(&seed0, &seed1);
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
		normal = cam.direction;
		mask = WHITE; // * dot(direction, normal)??
		
	}
	paths[index].origin = origin;
	paths[index].direction = direction;
	paths[index].mask = mask;
	paths[index].normal = normal;
	path_lengths[index] = 0;

	seeds[index] = seed0;
	seeds[index + 1] = seed1;
}
