//functions for compacting scene data to GPU format

// typedef struct bvh_struct
// {
// 	cl_float3 min; //spatial boundary
// 	cl_float3 max; //spatial boundary
// 	struct bvh_struct *left; 
// 	struct bvh_struct *right;
// 	Face *faces;
// 	int count;
// 	//maybe more?
// 	struct bvh_struct *next; //for tree building not for tracing
// }				tree_box;

// typedef struct meta_bvh_struct
// {
// 	cl_float3 min;
// 	cl_float3 max;
// 	struct meta_bvh_struct *left;
// 	struct meta_bvh_struct *right;
// 	tree_box *boxes;
// 	int count;
// 	struct meta_bvh_struct *next;
// }				meta_box;


/*
	gpu_data looks like this:
	V[], N[], T[], M[]
	faces are not a struct, just an index into these arrays. face i is at V[i * 3], T[i * 3], etc
	cl_float3 *V
	cl_float3 *N
	cl_float3 *T,
	cl_int3 *M (faces have a mat_type, mat_ind, and 3rd value tbd)

	struct gpu_box
	{
		float4 min
		float4 max //indices encoded in min.w max.w
	}
	is this enough? the idea is child indices are i * 2, i *2 + 1 unless min.w and max.w are nonzero,
	then they convert to int and are start end or start count into the faces
	we'll know from context whether we're in MB or DB
	it's tempting to try to make the MBs just have float3s and go to a specific depth but that doesn't actually improve things due to memory alignment and furthermore makes some reasonable but not guaranteed assumptions about the structure of the tree.


	So what gets passed in, and what goes first? we have a meta tree and an array of deep trees. the deep trees point into a flat array of what i'm calling whole faces, ie they have their v vt vn etc with them

	NB there isn't ONE array of faces, there's one per object.

	so I think the first thing to do is count the total faces, total boxes, etc.	
*/

gpu_scene *compact(meta_box *meta_bvh, tree_box **deep_bvhs)