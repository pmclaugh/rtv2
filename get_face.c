#include "rt.h"

int get_face_elements(char *line, int *va, int *vta, int *vna, int *vb, int *vtb, int *vnb, int *vc, int *vtc, int *vnc, int *vd, int *vtd, int *vnd)
{
	*va = *vta = *vna = *vb = *vtb = *vnb = *vc = *vtc = *vnc = *vd = *vtd = *vnd = 0;
	int vertices = 0;
	char *str = calloc(256, sizeof(char));

	++line;
	while (*line != '\0')
	{
		if (sscanf(line, " %s ", str) == 1)
			++vertices;
		else
			break;
		int elements = countwords(str, '/');
		int v, vt, vn;
		v = vt = vn = 0;
		if (elements != 3)
		{
			if (strstr(str, "//") == NULL)
				sscanf(str, "%d/%d", &v, &vt);
			else
				sscanf(str, "%d//%d", &v, &vn);
		}
		else
			sscanf(str, "%d/%d/%d", &v, &vt, &vn);
		if (vertices == 1)
		{
			*va = v;
			*vta = vt;
			*vna = vn;
		}
		else if (vertices == 2)
		{
			*vb = v;
			*vtb = vt;
			*vnb = vn;
		}
		else if (vertices == 3)
		{
			*vc = v;
			*vtc = vt;
			*vnc = vn;
		}
		else if (vertices == 4)
		{
			*vd = v;
			*vtd = vt;
			*vnd = vn;
		}
		line = move_str(line, str, 0);
	}
	free(str);
	return vertices;
}
