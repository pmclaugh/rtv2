#include "rt.h"
#include <fcntl.h>

t_object *new_simple_triangle(t_float3 vertex0, t_float3 vertex1, t_float3 vertex2, t_float3 color)
{
	t_object *triangle = calloc(1, sizeof(t_object));
	triangle->shape = TRIANGLE;
	triangle->position = vertex0;
	triangle->normal = vertex1;
	triangle->corner = vertex2;
	triangle->color = color;

	return triangle;
}

void unit_scale(t_import import, t_float3 offset)
{
	//find largest dimension
	t_float3 ranges = vec_sub(import.max, import.min);
	float scale = max3(ranges.x, ranges.y, ranges.z);
	scale = 1.0 / scale;
	t_object *obj = import.head;

	//for testing the negative problem
	offset = vec_scale(offset, -1.0);

	while (obj)
	{
		obj->position = vec_scale(vec_sub(obj->position, import.min), scale);
		obj->normal = vec_scale(vec_sub(obj->normal, import.min), scale);
		obj->corner = vec_scale(vec_sub(obj->corner, import.min), scale);
		obj->position = vec_add(obj->position, offset);
		obj->normal = vec_add(obj->normal, offset);
		obj->corner = vec_add(obj->corner, offset);
		obj = obj->next;
	}
}

t_import load_file(int ac, char **av)
{
	FILE *fp = fopen(av[1], "r");

	//scan through the file and count vertices and faces (we ignore normals)
	
	int vertices = 0;
	int faces = 0;
	char *type = malloc(10);
	float x;
	float y;
	float z;
	while(1)
	{
		if (fscanf(fp, "%s %f %f %f", type, &x, &y, &z) != 4)
			break;
		if (type[0] == 'v' && type[1] != 'f')
			vertices++;
		if (type[0] == 'f')
			faces++;
	}
	printf("%d vertices, %d faces\n", vertices, faces);

	fseek(fp, 0, SEEK_SET);

	t_float3 *verts = malloc(vertices * sizeof(t_float3));
	int v = 0;
	t_object *list = NULL;
	t_object *tail = NULL;
	t_float3 min = (t_float3){FLT_MAX, FLT_MAX, FLT_MAX};
	t_float3 max = (t_float3){FLT_MIN, FLT_MIN, FLT_MIN};
	while(1)
	{
		if (fscanf(fp, "%s %f %f %f", type, &x, &y, &z) != 4)
			break;
		if (type[0] == 'v' && type[1] != 'f')
		{
			verts[v++] = (t_float3){x, y, z};
			if (x < min.x)
				min.x = x;
			if (x > max.x)
				max.x = x;
			if (y < min.y)
				min.y = y;
			if (y > max.y)
				max.y = y;
			if (z < min.z)
				min.z = z;
			if (z > max.z)
				max.z = z;
		}
		if (type[0] == 'f')
		{
			t_object *this = new_simple_triangle(verts[(int)x - 1], verts[(int)y - 1], verts[(int)z - 1], GREEN);
			this->next = list;
			if (list == NULL)
				tail = this;
			list = this;
		}
	}
	printf("model loaded.\n");
	free(verts);
	return (t_import){list, tail, faces, min, max};
}