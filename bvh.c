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

cl_float3 bigmin;
cl_float3 bigmax;


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
	// if (max3(x,y,z) > 1.001 || min3(x,y,z) < -0.001)
	// 	printf("scaling is bad! %f %f %f\n", x, y, z);
	
	//printf("x %f y %f z %f\n", x, y, z);
	//x, y, z are in [0, 1]. multiply by 2^21 to get 21 bits, confine to [0, 2^21 - 1]
    x = fmin(fmax(x * 2097152.0f, 0.0f), 2097151.0f);
    y = fmin(fmax(y * 2097152.0f, 0.0f), 2097151.0f);
    z = fmin(fmax(z * 2097152.0f, 0.0f), 2097151.0f);
    return (mortonEncode_magicbits((unsigned int)x, (unsigned int)y, (unsigned int)z));
}

uint64_t morty_face(const Face *F)
{
	cl_float3 c = ORIGIN;
	for (int i = 0; i < F->shape; i++)
		c = vec_add(c, F->verts[i]);
	c = vec_scale(c, 1.0f / (float)F->shape);

	//scale that to unit cube
	c = vec_sub(c, bigmin);
	cl_float3 span = vec_sub(bigmax, bigmin);
	float scale = max3(span.x, span.y, span.z);
	c = (cl_float3){c.x / span.x, c.y / span.y, c.z / span.z};

	if (max3(c.x,c.y,c.z) > 1.001 || min3(c.x,c.y,c.z) < -0.001)
	{
		printf("scaling is bad! %f %f %f\n", c.x, c.y, c.z);
		printf("bounds:\n");
		print_clf3(bigmin);
		print_clf3(bigmax);
		for (int i = 0; i < F->shape; i++)
			print_clf3(F->verts[i]);
	}

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
	else if (ma == mb)
		return 0;
	else
		return -1;
}

void set_bounds(Box *B, Face *Faces)
{
	float max_x = -1.0 * FLT_MAX;
	float max_y = -1.0 * FLT_MAX;
	float max_z = -1.0 * FLT_MAX;

	float min_x = FLT_MAX;
	float min_y = FLT_MAX;
	float min_z = FLT_MAX; //never forget

	for (int i = B->start; i < B->end; i++)
	{
		for (int j = 0; j < Faces[i].shape; j++)
		{
			max_x = fmax(Faces[i].verts[j].x, max_x);
			max_y = fmax(Faces[i].verts[j].y, max_y);
			max_z = fmax(Faces[i].verts[j].z, max_z);

			min_x = fmin(Faces[i].verts[j].x, min_x);
			min_y = fmin(Faces[i].verts[j].y, min_y);
			min_z = fmin(Faces[i].verts[j].z, min_z);
		}
	}

	B->min = (cl_float3){min_x - 0.01f, min_y - 0.01f, min_z - 0.01f};
	B->max = (cl_float3){max_x + 0.01f, max_y + 0.01f, max_z + 0.01f};
}

int next_spot;

unsigned int morton_digit(uint64_t code, int depth)
{
	code = code << (1 + 3 * depth);
	code = code >> 61;
	//printf("sample morton code: %u\n", code);
	return code;
}

void tree_down(Box *Boxes, Face *Faces, int index, int depth)
{
	//take Boxes[index] and split it in 8, recurse on em
	//find midpoint w binary search (barrier between 011 and 100)
	//printf("depth %d\n", depth);
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

		if (step < 3 || depth > 20)
			return ;
		while(step > 0)
		{
			if (c == 7)
			{
				i = Boxes[index].end - 1;
				break;
			}
			step = step >> 1;
			unsigned int d = morton_digit(morty_face(&Faces[i]), depth);
			if (i + 1 == Boxes[index].end)
				break ;
			if (d <= c && morton_digit(morty_face(&Faces[i + 1]), depth) > c)
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
		tree_down(Boxes, Faces, next_spot, depth + 1);
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

void box_deets(Box b)
{
	printf("~~~~~~Box~~~~~~\n");
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

Box *bvh_obj(Face *Faces, int start, int end, int *boxcount)
{
	next_spot = 0;
	int count = end - start;
	Box *Boxes = calloc(count, sizeof(Box));

	Boxes[0] = (Box){
		ORIGIN,
		ORIGIN,
		start,
		end
	};

	set_bounds(&Boxes[0], Faces);
	bigmin = Boxes[0].min;
	bigmax = Boxes[0].max;

	//morton sort the faces
	qsort(&Faces[start], count, sizeof(Face), face_cmp);

	tree_down(Boxes, Faces, 0, 0);

	printf("%d faces %d boxes\n", count, next_spot + 1);
	*boxcount = next_spot + 1;
	return Boxes;
}

void gpu_ready_bvh(Scene *S, int *counts, int obj_count)
{
	Face *Faces = S->faces;

	Box **obj_trees = calloc(obj_count, sizeof(Box *));
	int box_total = 0;
	for (int i = 0; i < obj_count; i++)
	{
		int boxcount;
		if (i < obj_count - 1)
			obj_trees[i] = bvh_obj(Faces, counts[i], counts[i + 1], &boxcount);
		else
			obj_trees[i] = bvh_obj(Faces, counts[i], S->face_count, &boxcount);
		counts[i] = boxcount;
		box_total += boxcount;
	}
	printf("total box count:%d\n", box_total);

	//now we lay it flat and adjust child indices
	Box *deep_bvh = calloc(box_total, sizeof(Box));
	C_Box *shallow_bvh = calloc(obj_count, sizeof(C_Box));

	int start = 0;
	for (int i = 0; i < obj_count; i++)
	{
		for (int j = 0; j < counts[i]; j++)
		{
			Box b = obj_trees[i][j];
			for (int k = 0; k < b.children_count; k++)
				b.children[k] += start;
			deep_bvh[start + j] = b;
		}
		free(obj_trees[i]);
		shallow_bvh[i] = (C_Box){deep_bvh[start].min, deep_bvh[start].max, start};
		start += counts[i];
	}
	free(obj_trees);
	S->boxes = deep_bvh;
	S->box_count = box_total;
	S->c_boxes = shallow_bvh;
	S->c_box_count = obj_count;
}