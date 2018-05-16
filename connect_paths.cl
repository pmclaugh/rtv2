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

	float t, u, v;
	int ind = -1;

	float3 inv_dir = 1.0f / direction;

	if (camera_cos <= 0.0f || light_cos <= 0.0f)
		return 0;

	while (s_i)
	{
		int b_i = stack[--s_i];
		Box box = boxes[b_i];

		float this_t;
		int result = intersect_box(origin, inv_dir, box, t, &this_t);
		int inside = inside_box(origin, box);
		if (result && (inside || this_t < t))
		{
			if (b.rind < 0)
			{
				const int count = -1 * box.rind;
				const int start = -1 * box.lind;
				for (int i = start; i < start + count; i += 3)
					if (intersect_triangle(origin, direction, V, i, &ind, &t, &u, &v))
						break ;
			}
			else
			{
				Box l, r;
				l = boxes[box.lind];
				r = boxes[box.rind];
                float t_l = FLT_MAX;
                float t_r = FLT_MAX;
                int lhit = intersect_box(origin, inv_dir, l, t, &t_l);
                int rhit = intersect_box(origin, inv_dir, r, t, &t_r);
                int linside = inside_box(origin, l);
                int rinside = inside_box(origin, r);

                //this can be further improved
                if (lhit && (linside || t_l < t) && t_l >= t_r)
                    stack[s_i++] = box.lind;
                if (rhit && (rinside || t_r < t))
                    stack[s_i++] = box.rind;
                if (lhit && (linside || t_l < t) && t_l < t_r)
                    stack[s_i++] = box.lind;
			}
		}
	}

	if (ind == -1)
		return 1;
	return 0;
}

__kernel void connect_paths(__global Path *paths,
							__global int *path_lengths,
							__global Box *boxes,
							__global float3 *V,
							__global float3 *output)
{
	//remember threadcount issue
	int index = get_global_id(0);
	int row_size = get_global_size(0);

	int camera_length = path_lengths[index];
	int light_length = path_lengths[index + 1];

	float3 sum;

	for (int s = 0; s < camera_length; s++)
	{
		Path camera_vertex = paths[index + row_size * s];
		for (int t = 0; t < light_length; t++)
		{
			Path light_vertex = paths[index + 1 + row_size * t];

			float3 origin, direction, distance;
			origin = camera_vertex.origin;
			distance = light_vertex.origin - origin;
			t = native_sqrt(dot(distance, distance));
			direction = normalize(distance);

			float camera_cos = dot(camera_vertex.normal, direction);
			float light_cos = dot(light_vertex.normal, -1.0f * direction);

			if (camera_cos <= 0.0f || light_cos <= 0.0f)
				continue ;
			if (!visibility_test(origin, direction, t, boxes, V))
				continue ;

			sum += light_vertex.mask * camera_vertex.mask * BRIGHTNESS * camera_cos * light_cos;
		}
	}

	sum /= (float)(s * t);
	output[index] = sum;
}