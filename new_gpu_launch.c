#include "rt.h"
#include <fcntl.h>

typedef struct s_gpu_handle
{
	cl_kernel *init_paths;
	cl_kernel *trace_paths;
	cl_kernel *connect_paths;

	int light_poly_count;

	//memory areas
	cl_mem *d_outputs;
	cl_mem *d_seeds;
	cl_mem *d_paths;
	cl_mem *d_counts;

	cl_mem *d_vertexes;
	cl_mem *d_tex_coords;
	cl_mem *d_normal;
	cl_mem *d_tangent;
	cl_mem *d_bitangent;
	cl_mem *d_boxes;
	cl_mem *d_materials;
	cl_mem *d_tex;
	cl_mem *d_material_indices;
	cl_mem *d_lights;
}				gpu_handle;

typedef struct s_gpu_path{
	cl_float3 origin;
	cl_float3 direction;
	cl_float3 normal;
	cl_float3 mask;
	cl_float G;
	cl_float pC;
	cl_float pL;
	cl_int hit_light;
}				gpu_path;

typedef struct s_gpu_camera {
	cl_float3 pos;
	cl_float3 origin;
	cl_float3 direction;
	cl_float3 focus;
	cl_float3 d_x;
	cl_float3 d_y;
	cl_int width;
	cl_float focal_length;
	cl_float aperture;
}				gpu_camera;

char *load_cl_file(char *file)
{
	int fd = open(file, O_RDONLY);
	char *source = calloc(sizeof(char),30000); // real kernel is about 8k characters, whatever
	read(fd, source, 30000);
	close(fd);
	return source;
}

void reseed(gpu_scene *scene)
{
	cl_uint *h_seeds = calloc(scene->seed_count, sizeof(cl_uint));
	for (int i = 0; i < scene->seed_count; i++)
		h_seeds[i] = rand();
	free(scene->seeds);
	scene->seeds = h_seeds;
}

gpu_scene *prep_scene(Scene *s, gpu_context *CL, int worksize)
{
	//SEEDS
	cl_uint *h_seeds = calloc(worksize * 2 * CL->numDevices * CL->numPlatforms, sizeof(cl_uint));
	for (int i = 0; i < worksize * 2 * CL->numDevices; i++)
		h_seeds[i] = rand();


	//TEXTURES
	cl_int tex_size = 0;
	for (int i = 0; i < s->mat_count; i++)
	{
		if (s->materials[i].map_Kd)
			tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;
		if (s->materials[i].map_Ks)
			tex_size += s->materials[i].map_Ks->height * s->materials[i].map_Ks->width * 3;
		if (s->materials[i].map_Ke)
			tex_size += s->materials[i].map_Ke->height * s->materials[i].map_Ke->width * 3;
		if (s->materials[i].map_bump)
			tex_size += s->materials[i].map_bump->height * s->materials[i].map_bump->width * 3;
		if (s->materials[i].map_d)
			tex_size += s->materials[i].map_d->height * s->materials[i].map_d->width * 3;
	}

	cl_uchar *h_tex = calloc(sizeof(cl_uchar), tex_size);
	tex_size = 0;

	gpu_mat *simple_mats = calloc(s->mat_count, sizeof(gpu_mat));
	for (int i = 0; i < s->mat_count; i++)
	{
		simple_mats[i].Kd = s->materials[i].Kd;
		simple_mats[i].Ks = s->materials[i].Ks;
		simple_mats[i].Ke = s->materials[i].Ke;
		simple_mats[i].Ni = s->materials[i].Ni;
		simple_mats[i].Tr = s->materials[i].Tr;
		simple_mats[i].roughness = s->materials[i].roughness;
		printf("gpu mat %d will be a=%.2f, Ni=%.1f, Tr=%.1f\n", i, simple_mats[i].roughness, simple_mats[i].Ni, simple_mats[i].Tr);

		if (s->materials[i].map_Kd)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_Kd->pixels, s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3);
			simple_mats[i].diff_ind = tex_size;
			simple_mats[i].diff_h = s->materials[i].map_Kd->height;
			simple_mats[i].diff_w = s->materials[i].map_Kd->width;
			tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;
		}

		if (s->materials[i].map_Ks)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_Ks->pixels, s->materials[i].map_Ks->height * s->materials[i].map_Ks->width * 3);
			simple_mats[i].spec_ind = tex_size;
			simple_mats[i].spec_h = s->materials[i].map_Ks->height;
			simple_mats[i].spec_w = s->materials[i].map_Ks->width;
			tex_size += s->materials[i].map_Ks->height * s->materials[i].map_Ks->width * 3;
		}

		if (s->materials[i].map_Ke)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_Ke->pixels, s->materials[i].map_Ke->height * s->materials[i].map_Ke->width * 3);
			simple_mats[i].emiss_ind = tex_size;
			simple_mats[i].emiss_h = s->materials[i].map_Ke->height;
			simple_mats[i].emiss_w = s->materials[i].map_Ke->width;
			tex_size += s->materials[i].map_Ke->height * s->materials[i].map_Ke->width * 3;
		}

		if (s->materials[i].map_bump)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_bump->pixels, s->materials[i].map_bump->height * s->materials[i].map_bump->width * 3);
			simple_mats[i].bump_ind = tex_size;
			simple_mats[i].bump_h = s->materials[i].map_bump->height;
			simple_mats[i].bump_w = s->materials[i].map_bump->width;
			tex_size += s->materials[i].map_bump->height * s->materials[i].map_bump->width * 3;
		}

		if (s->materials[i].map_d)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_d->pixels, s->materials[i].map_d->height * s->materials[i].map_d->width * 3);
			simple_mats[i].trans_ind = tex_size;
			simple_mats[i].trans_h = s->materials[i].map_d->height;
			simple_mats[i].trans_w = s->materials[i].map_d->width;
			tex_size += s->materials[i].map_d->height * s->materials[i].map_d->width * 3;
		}
	}


	//FACES
	cl_float3 *V, *T, *N;
	V = calloc(s->face_count * 3, sizeof(cl_float3));
	T = calloc(s->face_count * 3, sizeof(cl_float3));
	N = calloc(s->face_count * 3, sizeof(cl_float3));
	cl_int *M;
	M = calloc(s->face_count, sizeof(cl_int));
	cl_float3 *TN, *BTN;
	TN = calloc(s->face_count, sizeof(cl_float3));
	BTN = calloc(s->face_count, sizeof(cl_float3));

	cl_int light_poly_count = 0;
	for (int i = 0; i < s->face_count; i++)
	{
		Face f = s->faces[i];

		V[i * 3] = f.verts[0];
		V[i * 3 + 1] = f.verts[1];
		V[i * 3 + 2] = f.verts[2];

		T[i * 3] = f.tex[0];
		T[i * 3 + 1] = f.tex[1];
		T[i * 3 + 2] = f.tex[2];

		N[i * 3] = f.norms[0];
		N[i * 3 + 1] = f.norms[1];
		N[i * 3 + 2] = f.norms[2];

		M[i] = f.mat_ind;
		if (simple_mats[f.mat_ind].bump_h)
		{
			cl_float3 dp1 = vec_sub(f.verts[1], f.verts[0]);
			cl_float3 dp2 = vec_sub(f.verts[2], f.verts[0]);
			cl_float3 duv1 = vec_sub(f.tex[1], f.tex[0]);
			cl_float3 duv2 = vec_sub(f.tex[2], f.tex[0]);
			float r = duv1.x * duv2.y - duv1.y * duv2.x == 0.0f ? 1.0f : 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);

			TN[i] = unit_vec(vec_scale(vec_sub(vec_scale(dp1, duv2.y), vec_scale(dp2, duv1.y)), r));
			TN[i] = unit_vec(vec_scale(vec_sub(vec_scale(dp1, duv2.y), vec_scale(dp2, duv1.y)), r));
			BTN[i] = unit_vec(cross(TN[i], cross(vec_sub(f.verts[1], f.verts[0]), vec_sub(f.verts[2], f.verts[0]))));
		}
		if (dot(simple_mats[f.mat_ind].Ke, simple_mats[f.mat_ind].Ke) > 0.0f)
			light_poly_count++;
	}

	printf("lights\n");
	cl_int3 *lights = calloc(light_poly_count, sizeof(cl_int3));
	light_poly_count = 0;
	for (int i = 0; i < s->face_count; i++)
	{
		Face f = s->faces[i];
		if (dot(simple_mats[f.mat_ind].Ke, simple_mats[f.mat_ind].Ke) > 0.0f)
			lights[light_poly_count++] = (cl_int3){i * 3, i * 3 + 1, i * 3 + 2};
	}

	printf("lights done\n");
	//BINS
	gpu_bin *flat_bvh = flatten_bvh(s);

	//COMBINE
	gpu_scene *gs = calloc(1, sizeof(gpu_scene));
	*gs = (gpu_scene){V, T, N, M, TN, BTN, s->face_count * 3, lights, light_poly_count, flat_bvh, s->bin_count, h_tex, tex_size, simple_mats, s->mat_count, h_seeds, worksize * 2 * CL->numDevices * CL->numPlatforms};
	return gs;
}

gpu_context *prep_gpu(void)
{
	// printf("prepping for GPU launch\n");
	gpu_context *gpu = calloc(1, sizeof(gpu_context));
    int err;

    // Find number of platforms
    err = clGetPlatformIDs(0, NULL, &gpu->numPlatforms);

    // Get all platforms
    gpu->platform = calloc(gpu->numPlatforms, sizeof(cl_platform_id));
    err = clGetPlatformIDs(gpu->numPlatforms, gpu->platform, NULL);

    //count devices
    gpu->numDevices = 0;
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	cl_uint d;
        err = clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_GPU, 0, NULL, &d);
        if (err == CL_SUCCESS)
            gpu->numDevices += d;
    }

    printf("%u devices, %u platforms\n", gpu->numDevices, gpu->numPlatforms);

    //get ids for devices and create (platforms) compute contexts, (devices) command queues
	cl_device_id *device_ids = calloc(gpu->numDevices, sizeof(cl_device_id));
	gpu->device_ids = device_ids;
    cl_uint offset = 0;
    gpu->contexts = calloc(gpu->numPlatforms, sizeof(cl_context));
    gpu->commands = calloc(gpu->numDevices, sizeof(cl_command_queue));
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	cl_uint d;
    	clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_GPU, gpu->numDevices, &device_ids[offset], &d);
    	gpu->contexts[i] = clCreateContext(0, d, &device_ids[offset], NULL, NULL, &err);
    	for (int j = 0; j < d; j++)
    	{
    		// #ifndef __APPLE__
    		// 	gpu->commands[offset + j] = clCreateCommandQueueWithProperties(gpu->contexts[i], device_ids[offset + j], NULL, &err);
    		// #endif
    		// #ifdef __APPLE__
    			gpu->commands[offset + j] = clCreateCommandQueue(gpu->contexts[i], device_ids[offset + j], CL_QUEUE_PROFILING_ENABLE, &err);
    		// #endif
    	}
    	offset += d;
    }
    // printf("made contexts and CQs\n");

    char *source = load_cl_file("bdpt_full.cl");
    printf("loaded kernel source\n");

    //create (platforms) programs and build them
    gpu->programs = calloc(gpu->numPlatforms, sizeof(cl_program));
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	gpu->programs[i] = clCreateProgramWithSource(gpu->contexts[i], 1, (const char **) &source, NULL, &err);
    	err = clBuildProgram(gpu->programs[i], 0, NULL, NULL, NULL, NULL);
    	if (err != CL_SUCCESS)
    	{
    	 	printf("bad compile\n");
    	 	char *build_log;
			size_t ret_val_size = 0;
			clGetProgramBuildInfo(gpu->programs[i], device_ids[i], CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
			printf("ret val %lu\n", ret_val_size);
			build_log = malloc(ret_val_size + 1);
			clGetProgramBuildInfo(gpu->programs[i], device_ids[i], CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
			printf("%s\n", build_log);
			free(build_log);
    	 	exit(0);
    	}
	    free(source);
    }
    printf("good compile\n");
    return gpu;
}

void recompile(gpu_context *CL, gpu_handle *handle)
{
	cl_int err;
	//release old program(s)
	for (int i = 0; i < CL->numPlatforms; i++)
		clReleaseProgram(CL->programs[i]);

	char *source = load_cl_file("bdpt_full.cl");
	//printf("loaded kernel source\n");

    //create (platforms) programs and build them
    CL->programs = calloc(CL->numPlatforms, sizeof(cl_program));
	for (int i = 0; i < CL->numPlatforms; i++)
	{
		CL->programs[i] = clCreateProgramWithSource(CL->contexts[i], 1, (const char **) &source, NULL, &err);
		err = clBuildProgram(CL->programs[i], 0, NULL, NULL, NULL, NULL);
		if (err != CL_SUCCESS)
		{
			printf("bad compile\n");
			char *build_log;
			size_t ret_val_size = 0;
			clGetProgramBuildInfo(CL->programs[i], CL->device_ids[i], CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
			printf("ret val %lu\n", ret_val_size);
			build_log = malloc(ret_val_size + 1);
			clGetProgramBuildInfo(CL->programs[i], CL->device_ids[i], CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
			printf("%s\n", build_log);
			free(build_log);
			exit(0);
		}
		free(source);
	}
	printf("good compile\n");
	//free old kernels
	for (int i = 0 ; i < CL->numDevices; i++)
	{
		clReleaseKernel(handle->init_paths[i]);
		clReleaseKernel(handle->trace_paths[i]);
		clReleaseKernel(handle->connect_paths[i]);
	}

	//recreate kernels

	for (int i = 0; i < CL->numDevices; i++)
	{
		handle->init_paths[i] = clCreateKernel(CL->programs[0], "init_paths", &err);
		handle->trace_paths[i] = clCreateKernel(CL->programs[0], "trace_paths", &err);
		handle->connect_paths[i] = clCreateKernel(CL->programs[0], "connect_paths", &err);
	}

	//reconnect arguments
	for (int i = 0; i < CL->numDevices; i++)
	{
		/*
		__kernel void init_paths(const Camera cam,
						__global uint *seeds,
						__global Path *paths,
						__global int *path_lengths,
						__global int3 *lights,
						const uint light_poly_count,
						__global float3 *V,
						__global float3 *N,
						__global float3 *output
						)
		*/
		clSetKernelArg(handle->init_paths[i], 1, sizeof(cl_mem), &handle->d_seeds[i]);
		clSetKernelArg(handle->init_paths[i], 2, sizeof(cl_mem), &handle->d_paths[i]);
		clSetKernelArg(handle->init_paths[i], 3, sizeof(cl_mem), &handle->d_counts[i]);
		clSetKernelArg(handle->init_paths[i], 4, sizeof(cl_mem), &handle->d_lights[i]);
		clSetKernelArg(handle->init_paths[i], 5, sizeof(cl_uint), &handle->light_poly_count);
		clSetKernelArg(handle->init_paths[i], 6, sizeof(cl_mem), &handle->d_vertexes[i]);
		clSetKernelArg(handle->init_paths[i], 7, sizeof(cl_mem), &handle->d_normal[i]);
		clSetKernelArg(handle->init_paths[i], 8, sizeof(cl_mem), &handle->d_outputs[i]);
		clSetKernelArg(handle->init_paths[i], 9, sizeof(cl_mem), &handle->d_material_indices[i]);
		clSetKernelArg(handle->init_paths[i], 10, sizeof(cl_mem), &handle->d_materials[i]);
		/*
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
		*/
		clSetKernelArg(handle->trace_paths[i], 0, sizeof(cl_mem), &handle->d_paths[i]);
		clSetKernelArg(handle->trace_paths[i], 1, sizeof(cl_mem), &handle->d_vertexes[i]);
		clSetKernelArg(handle->trace_paths[i], 2, sizeof(cl_mem), &handle->d_tex_coords[i]);
		clSetKernelArg(handle->trace_paths[i], 3, sizeof(cl_mem), &handle->d_normal[i]);
		clSetKernelArg(handle->trace_paths[i], 4, sizeof(cl_mem), &handle->d_tangent[i]);
		clSetKernelArg(handle->trace_paths[i], 5, sizeof(cl_mem), &handle->d_bitangent[i]);
		clSetKernelArg(handle->trace_paths[i], 6, sizeof(cl_mem), &handle->d_material_indices[i]);
		clSetKernelArg(handle->trace_paths[i], 7, sizeof(cl_mem), &handle->d_materials[i]);
		clSetKernelArg(handle->trace_paths[i], 8, sizeof(cl_mem), &handle->d_tex[i]);
		clSetKernelArg(handle->trace_paths[i], 9, sizeof(cl_mem), &handle->d_counts[i]);
		clSetKernelArg(handle->trace_paths[i], 10, sizeof(cl_mem), &handle->d_boxes[i]);
		clSetKernelArg(handle->trace_paths[i], 11, sizeof(cl_mem), &handle->d_seeds[i]);
		clSetKernelArg(handle->trace_paths[i], 12, sizeof(cl_mem), &handle->d_outputs[i]);


		/*
		__kernel void connect_paths(__global Path *paths,
							__global int *path_lengths,
							__global Box *boxes,
							__global float3 *V,
							__global float3 *output)
		*/
		clSetKernelArg(handle->connect_paths[i], 0, sizeof(cl_mem), &handle->d_paths[i]);
		clSetKernelArg(handle->connect_paths[i], 1, sizeof(cl_mem), &handle->d_counts[i]);
		clSetKernelArg(handle->connect_paths[i], 2, sizeof(cl_mem), &handle->d_boxes[i]);
		clSetKernelArg(handle->connect_paths[i], 3, sizeof(cl_mem), &handle->d_vertexes[i]);
		clSetKernelArg(handle->connect_paths[i], 4, sizeof(cl_mem), &handle->d_outputs[i]);
	}
}

gpu_handle *gpu_alloc(gpu_context *CL, gpu_scene *scene, int worksize)
{

	gpu_handle *handle = calloc(1, sizeof(gpu_handle));

	int half_worksize = worksize / 2;

	handle->light_poly_count = scene->light_poly_count;
	//alloc and copy memory

	//per-device
	handle->d_outputs = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_seeds = 	calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_paths = 	calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_counts = 	calloc(CL->numDevices, sizeof(cl_mem));

	//formerly per-platform now per-device
	handle->d_vertexes = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_tex_coords = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_normal = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_tangent = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_bitangent = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_boxes = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_materials = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_tex = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_material_indices = calloc(CL->numDevices, sizeof(cl_mem));
	handle->d_lights = calloc(CL->numDevices, sizeof(cl_mem));
	
	cl_float3 *blank_output = calloc(half_worksize, sizeof(cl_float3));
	gpu_path *empty_rays = calloc(worksize * 10, sizeof(gpu_path));
	cl_int *zero_counts = calloc(worksize, sizeof(cl_int));

	for (int i = 0; i < CL->numDevices; i++)
	{
		handle->d_seeds[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint) * 2 * worksize, &scene->seeds[2 * worksize * i], NULL);
		handle->d_outputs[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * half_worksize, blank_output, NULL);
		handle->d_paths[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(gpu_path) * worksize * 10, empty_rays, NULL);
		handle->d_counts[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_int) * worksize, zero_counts, NULL);

		handle->d_vertexes[i] = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count, scene->V, NULL);
		handle->d_tex_coords[i] = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count, scene->T, NULL);
		handle->d_normal[i] = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count, scene->N, NULL);
		handle->d_tangent[i] = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count / 3, scene->TN, NULL);
		handle->d_bitangent[i] = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count / 3, scene->BTN, NULL);
		handle->d_boxes[i] = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(gpu_bin) * scene->bin_count, scene->bins, NULL);
		handle->d_materials[i] = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(gpu_mat) * scene->mat_count, scene->mats, NULL);
		handle->d_tex[i] = 				clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_uchar) * scene->tex_size, scene->tex, NULL);
		handle->d_material_indices[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_int) * scene->tri_count / 3, scene->M, NULL);
		handle->d_lights[i] = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_int3) * scene->light_poly_count, scene->lights, NULL);
	}

	free(blank_output);
	free(empty_rays);
	free(zero_counts);

	cl_int err;

	//alloc and compile kernels

	handle->init_paths = calloc(CL->numDevices, sizeof(cl_kernel));
	handle->trace_paths = calloc(CL->numDevices, sizeof(cl_kernel));
	handle->connect_paths = calloc(CL->numDevices, sizeof(cl_kernel));

	for (int i = 0; i < CL->numDevices; i++)
	{
		handle->init_paths[i] = clCreateKernel(CL->programs[0], "init_paths", &err);
		handle->trace_paths[i] = clCreateKernel(CL->programs[0], "trace_paths", &err);
		handle->connect_paths[i] = clCreateKernel(CL->programs[0], "connect_paths", &err);
	}

	//connect arguments
	for (int i = 0; i < CL->numDevices; i++)
	{
		/*
		__kernel void init_paths(const Camera cam,
						__global uint *seeds,
						__global Path *paths,
						__global int *path_lengths,
						__global int3 *lights,
						const uint light_poly_count,
						__global float3 *V,
						__global float3 *N,
						__global float3 *output
						)
		*/
		clSetKernelArg(handle->init_paths[i], 1, sizeof(cl_mem), &handle->d_seeds[i]);
		clSetKernelArg(handle->init_paths[i], 2, sizeof(cl_mem), &handle->d_paths[i]);
		clSetKernelArg(handle->init_paths[i], 3, sizeof(cl_mem), &handle->d_counts[i]);
		clSetKernelArg(handle->init_paths[i], 4, sizeof(cl_mem), &handle->d_lights[i]);
		clSetKernelArg(handle->init_paths[i], 5, sizeof(cl_uint), &handle->light_poly_count);
		clSetKernelArg(handle->init_paths[i], 6, sizeof(cl_mem), &handle->d_vertexes[i]);
		clSetKernelArg(handle->init_paths[i], 7, sizeof(cl_mem), &handle->d_normal[i]);
		clSetKernelArg(handle->init_paths[i], 8, sizeof(cl_mem), &handle->d_outputs[i]);
		clSetKernelArg(handle->init_paths[i], 9, sizeof(cl_mem), &handle->d_material_indices[i]);
		clSetKernelArg(handle->init_paths[i], 10, sizeof(cl_mem), &handle->d_materials[i]);
		/*
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
		*/
		clSetKernelArg(handle->trace_paths[i], 0, sizeof(cl_mem), &handle->d_paths[i]);
		clSetKernelArg(handle->trace_paths[i], 1, sizeof(cl_mem), &handle->d_vertexes[i]);
		clSetKernelArg(handle->trace_paths[i], 2, sizeof(cl_mem), &handle->d_tex_coords[i]);
		clSetKernelArg(handle->trace_paths[i], 3, sizeof(cl_mem), &handle->d_normal[i]);
		clSetKernelArg(handle->trace_paths[i], 4, sizeof(cl_mem), &handle->d_tangent[i]);
		clSetKernelArg(handle->trace_paths[i], 5, sizeof(cl_mem), &handle->d_bitangent[i]);
		clSetKernelArg(handle->trace_paths[i], 6, sizeof(cl_mem), &handle->d_material_indices[i]);
		clSetKernelArg(handle->trace_paths[i], 7, sizeof(cl_mem), &handle->d_materials[i]);
		clSetKernelArg(handle->trace_paths[i], 8, sizeof(cl_mem), &handle->d_tex[i]);
		clSetKernelArg(handle->trace_paths[i], 9, sizeof(cl_mem), &handle->d_counts[i]);
		clSetKernelArg(handle->trace_paths[i], 10, sizeof(cl_mem), &handle->d_boxes[i]);
		clSetKernelArg(handle->trace_paths[i], 11, sizeof(cl_mem), &handle->d_seeds[i]);
		clSetKernelArg(handle->trace_paths[i], 12, sizeof(cl_mem), &handle->d_outputs[i]);


		/*
		__kernel void connect_paths(__global Path *paths,
							__global int *path_lengths,
							__global Box *boxes,
							__global float3 *V,
							__global float3 *output,
							const Camera cam)
		*/
		clSetKernelArg(handle->connect_paths[i], 0, sizeof(cl_mem), &handle->d_paths[i]);
		clSetKernelArg(handle->connect_paths[i], 1, sizeof(cl_mem), &handle->d_counts[i]);
		clSetKernelArg(handle->connect_paths[i], 2, sizeof(cl_mem), &handle->d_boxes[i]);
		clSetKernelArg(handle->connect_paths[i], 3, sizeof(cl_mem), &handle->d_vertexes[i]);
		clSetKernelArg(handle->connect_paths[i], 4, sizeof(cl_mem), &handle->d_outputs[i]);
	}

	return handle;
}

cl_float3 *gpu_render(Scene *S, t_camera cam, int xdim, int ydim, int samples, int min_bounces, int first, cl_int **count_out)
{
	size_t half_worksize = xdim * ydim;
	size_t worksize =  half_worksize * 2;
	
	size_t localsize = 64;
	size_t sample_max = samples;
	size_t width = xdim;

	static gpu_context *CL;
	static gpu_scene *scene;
	static gpu_handle *handle;
	if (!CL)
		CL = prep_gpu();
	else if (first)
		recompile(CL, handle);
	
	if (!scene)
		scene = prep_scene(S, CL, worksize);

	if (!handle)
		handle = gpu_alloc(CL, scene, worksize);


	gpu_camera gcam;
	gcam.pos = cam.pos;
	gcam.focus = cam.focus;
	gcam.origin = cam.origin;
	gcam.direction = cam.dir;
	gcam.d_x = cam.d_x;
	gcam.d_y = cam.d_y;
	gcam.width = xdim;
	gcam.focal_length = cam.focal_length;
	gcam.aperture = cam.aperture;

	//camera may need update
	for (int i = 0; i < CL->numDevices; i++)
	{
		clSetKernelArg(handle->init_paths[i], 0, sizeof(gpu_camera), &gcam);
		clSetKernelArg(handle->connect_paths[i], 5, sizeof(gpu_camera), &gcam);
	}

	//ACTUAL LAUNCH TIME
	cl_event begin, finish;
	cl_ulong start, end;
	cl_event init, trace, connect;
	for (int i = 0; i < CL->numDevices; i++)
		for (int j = 0; j < samples; j++)
		{
			clEnqueueNDRangeKernel(CL->commands[i], handle->init_paths[i], 1, 0, &worksize, &localsize, j == 0 ? 0 : 1, j == 0 ? NULL : &connect, &init);
			if (j == 0 && i == 0)
				begin = init;
			clEnqueueNDRangeKernel(CL->commands[i], handle->trace_paths[i], 1, 0, &worksize, &localsize, 1, &init, &trace);
			clEnqueueNDRangeKernel(CL->commands[i], handle->connect_paths[i], 1, 0, &half_worksize, &localsize, 1, &trace, &connect);
			if (j == samples - 1 && i == CL->numDevices - 1)
				finish = connect;
		}

	for (int i = 0; i < CL->numDevices; i++)
		clFlush(CL->commands[i]);

	for (int i = 0; i < CL->numDevices; i++)
		clFinish(CL->commands[i]);

	// printf("made it out of kernel\n");

	clGetEventProfilingInfo(begin, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
	clGetEventProfilingInfo(finish, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
	printf("took %.3f seconds\n", (float)(end - start) / 1000000000.0f);
	clReleaseEvent(begin);
	clReleaseEvent(finish);

	cl_float3 **outputs = calloc(CL->numDevices, sizeof(cl_float3 *));
	for (int i = 0; i < CL->numDevices; i++)
		outputs[i] = calloc(half_worksize, sizeof(cl_float3));

	cl_int **counts = calloc(CL->numDevices, sizeof(cl_int *));
	for (int i = 0; i < CL->numDevices; i++)
		counts[i] = calloc(worksize, sizeof(cl_int));

	for (int i = 0; i < CL->numDevices; i++)
	{
		clEnqueueReadBuffer(CL->commands[i], handle->d_outputs[i], CL_TRUE, 0, half_worksize * sizeof(cl_float3), outputs[i], 0, NULL, NULL);
		clEnqueueReadBuffer(CL->commands[i], handle->d_counts[i], CL_TRUE, 0, worksize * sizeof(cl_int), counts[i], 0, NULL, NULL);
		clEnqueueReadBuffer(CL->commands[i], handle->d_seeds[i], CL_TRUE, 0, 2 * worksize * sizeof(cl_uint), &scene->seeds[i * 2 * worksize], 0, NULL, NULL);
	}
	for (int i = 0; i < CL->numDevices; i++)
		clFinish(CL->commands[i]);

	cl_float3 *output = calloc(half_worksize, sizeof(cl_float3));
	cl_int *count = calloc(half_worksize, sizeof(cl_int));
	cl_int light_count = 0;
	cl_int camera_count = 0;
	for (int i = 0; i < CL->numDevices; i++)
		for (int j = 0; j < half_worksize; j++)
		{
			output[j] = vec_add(output[j], outputs[i][j]);
			count[j] += 1;//counts[i][2 * j] * counts[i][2 * j + 1];
			camera_count += counts[i][2 * j];
			light_count += counts[i][2 * j + 1];
		}

	printf("average camera path length, %f\n", (float)camera_count / (float)(half_worksize * CL->numDevices));
	printf("average light path length, %f\n", (float)light_count / (float)(half_worksize * CL->numDevices));
	for (int i = 0; i < CL->numDevices; i++)
	{
		free(counts[i]);
		free(outputs[i]);
	}

	free(counts);
	free(outputs);

	*count_out = count;
	return output;
}
