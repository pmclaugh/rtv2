#include "rt.h"

#define ORIGIN (cl_float3){0.0f, 0.0f, 0.0f}

float min3(float a, float b, float c)
{
	return fmin(fmin(a, b), c);
}

float max3(float a, float b, float c)
{
	return fmax(fmax(a, b), c);
}


uint64_t splitBy3(const unsigned int a)
{
    uint64_t x = a & 0x1fffff; // we only look at the first 21 bits
    x = (x | x << 32) & 0x1f00000000ffff;  // shift left 32 bits, OR with self, and 00011111000000000000000000000000000000001111111111111111
    x = (x | x << 16) & 0x1f0000ff0000ff;  // shift left 32 bits, OR with self, and 00011111000000000000000011111111000000000000000011111111
    x = (x | x << 8) & 0x100f00f00f00f00f; // shift left 32 bits, OR with self, and 0001000000001111000000001111000000001111000000001111000000000000
    x = (x | x << 4) & 0x10c30c30c30c30c3; // shift left 32 bits, OR with self, and 0001000011000011000011000011000011000011000011000011000100000000
    x = (x | x << 2) & 0x1249249249249249;
    return x;
}
 
uint64_t mortonEncode_magicbits(const unsigned int x, const unsigned int y, const unsigned int z)
{
	//interweave the rightmost 21 bits of 3 unsigned ints to generate a 63-bit morton code
    uint64_t answer = 0;
    answer |= splitBy3(z) | splitBy3(y) << 1 | splitBy3(x) << 2;
    return answer;
}

uint64_t morton64(float x, float y, float z)
{
	if (max3(x,y,z) > 1 || min3(x,y,z) < 0)
		printf("scaling is bad!\n");
	//printf("x %f y %f z %f\n", x, y, z);
	//x, y, z are in [0, 1]. multiply by 2^21 to get 21 bits, confine to [0, 2^21 - 1]
    x = fmin(fmax(x * 2097152.0f, 0.0f), 2097151.0f);
    y = fmin(fmax(y * 2097152.0f, 0.0f), 2097151.0f);
    z = fmin(fmax(z * 2097152.0f, 0.0f), 2097151.0f);
    return (mortonEncode_magicbits((unsigned int)x, (unsigned int)y, (unsigned int)z));
}

cl_float3 bigmin;
cl_float3 bigmax;

uint64_t morty_face(const Face *F)
{
	cl_float3 c = ORIGIN;
	for (int i = 0; i < F->shape; i++)
		c = vec_add(c, F->verts[i]);
	c = vec_scale(c, 1.0f / (float)F->shape);

	//scale that to unit cube
	c = vec_sub(c, bigmin);
	cl_float3 span = vec_sub(bigmax, bigmin);
	c = (cl_float3){c.x / span.x, c.y / span.y, c.z / span.z};

	return morton64(c.x, c.y, c.z);
}

int face_cmp(const void *a, const void *b)
{
	Face *f_a = (Face *)a;
	Face *f_b = (Face *)b;
	uint64_t ma = morty_face(f_a);
	uint64_t mb = morty_face(f_b);
	if (ma > mb)
		return 1;
	if (ma == mb)
		return 0;
	if (ma < mb)
		return -1;
}

void set_bounds(Box *B, Face *Faces)
{
	float max_x = FLT_MAX;
	float max_y = FLT_MAX;
	float max_z = FLT_MAX;

	float min_x = -1.0 * FLT_MAX;
	float min_y = -1.0 * FLT_MAX;
	float min_z = -1.0 * FLT_MAX; //never forget

	for (int i = B->start; i < B->end; i++)
		for (int j = 0; j < Faces[i].shape; j++)
		{
			max_x = fmin(Faces[i].verts[j].x, max_x);
			max_y = fmin(Faces[i].verts[j].y, max_y);
			max_z = fmin(Faces[i].verts[j].z, max_z);

			min_x = fmax(Faces[i].verts[j].x, min_x);
			min_y = fmax(Faces[i].verts[j].y, min_y);
			min_z = fmax(Faces[i].verts[j].z, min_z);
		}

	B->min = (cl_float3){min_x, min_y, min_z};
	B->max = (cl_float3){max_x, max_y, max_z};
}

int next_spot;

unsigned int morton_digit(uint64_t code, int depth)
{
	code = code << (1 + 3 * depth);
	code = code >> 61;
	//printf("sample morton code: %u\n", code);
	return code;
}

void tree_down(Box *Boxes, Face *Faces, uint64_t *codes, int index, int depth)
{
	//take Boxes[index] and split it in 8, recurse on em
	//find midpoint w binary search (barrier between 011 and 100)
	printf("depth %d\n", depth);
	if (Boxes[index].start == Boxes[index].end)
	{
		next_spot--;
		return;
	}

	int start = Boxes[index].start;
	for (int c = 0; c < 8; c++)
	{
		int step = (Boxes[index].end - Boxes[index].start) >> 1;
		int i = Boxes[index].start + step;

		if (step < 2 || depth > 20)
			return ;
		while(step > 0)
		{
			if (c == 7)
			{
				i = Boxes[index].end - 1;
				break;
			}
			step = step >> 1;
			unsigned int d = morton_digit(codes[i], depth);
			if (d <= c && morton_digit(codes[i + 1], depth) > c)
				break ;
			if (d <= c)
				i += step;
			else
				i -= step;
		}
		next_spot++;
		Boxes[index].children[c] = next_spot;
		Boxes[next_spot] = (Box){
			ORIGIN,
			ORIGIN,
			start,
			i + 1
		};
		set_bounds(&Boxes[next_spot], Faces);
		tree_down(Boxes, Faces, codes, next_spot, depth + 1);
		start = i + 1;
	}

	int unique[8];
	int count = 0;
	for (int i = 0; i < 8; i++)
		if (count == 0 || Boxes[index].children[i] != unique[count - 1])
			unique[count++] = Boxes[index].children[i];
	memcpy(&Boxes[index].children, &unique, sizeof(int) * 8);
	Boxes[index].children_count = count;
}

void box_deets(Box b, int i)
{
	printf("~~~~~~Box %d~~~~~~\n", i);
	printf("min: ");
	print_clf3(b.min);
	printf("max: ");
	print_clf3(b.max);
	printf("start %d end %d (%d)\n", b.start, b.end, b.end - b.start);
	printf("%d children: ", b.children_count);
	for (int i = 0; i < b.children_count; i++)
		printf("%d ", b.children[i]);
	printf("\n");
}

void gpu_bvh(Scene *S)
{
	//REMEMBER this can be optimized later, for now just make it work
	Face *Faces = S->faces;
	Box *Boxes = calloc(S->face_count, sizeof(Box));
	uint64_t *codes = calloc(S->face_count, sizeof(uint64_t));

	Boxes[0] = (Box){
		ORIGIN,
		ORIGIN,
		0,
		S->face_count
	};

	set_bounds(&Boxes[0], Faces);
	bigmin = Boxes[0].min;
	bigmax = Boxes[0].max;
	print_clf3(bigmin);
	print_clf3(bigmax);
	//morton sort the faces
	qsort(Faces, S->face_count, sizeof(Face), face_cmp);
	//cache codes
	for (int i = 0; i < S->face_count; i++)
		codes[i] = morty_face(&Faces[i]);

	next_spot = 0;

	tree_down(Boxes, Faces, codes, 0, 0);

	S->boxes = Boxes;
	S->box_count = next_spot + 1;

	printf("I think we did the boxes. %d of them\n", S->box_count);
	// for (int i = 0; i < S->box_count; i++)
	// 	box_deets(Boxes[i], i);
}