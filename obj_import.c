#include "rt.h"
#include <fcntl.h>

void load_file(t_scene *scene, int ac, char **av)
{
	if (ac != 2)
	{
		printf("bad arguments maybe?\n");
		return;
	}

	FILE *fp = fopen(av[1], "r");
	if (!fp)
	{
		printf("bad filename maybe?\n");
		return;
	}

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
	while(1)
	{
		if (fscanf(fp, "%s %f %f %f", type, &x, &y, &z) != 4)
			break;
		if (type[0] == 'v' && type[1] != 'f')
			verts[v++] = (t_float3){x, y, z};
		if (type[0] == 'f')
			new_triangle(scene, verts[(int)x - 1], verts[(int)y - 1], verts[(int)z - 1], GREEN);
	}
	printf("model loaded.\n");
}