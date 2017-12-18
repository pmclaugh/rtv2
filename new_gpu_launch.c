#include "rt.h"
#include <fcntl.h>

#define SAMPLES_PER_DEVICE 10

char *load_cl_file(char *file)
{
	int fd = open(file, O_RDONLY);
	char *source = calloc(sizeof(char),30000); // real kernel is about 8k characters, whatever
	read(fd, source, 30000);
	close(fd);
	return source;
}

gpu_scene *prep_scene(Scene *s, gpu_context *CL, int xdim, int ydim)
{
	//FACES
	cl_float3 *V, *T, *N;
	V = calloc(s->face_count * 3, sizeof(cl_float3));
	T = calloc(s->face_count * 3, sizeof(cl_float3));
	N = calloc(s->face_count * 3, sizeof(cl_float3));

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
	}

	//SEEDS
	cl_uint *h_seeds = calloc(xdim * ydim * 2 * CL->numDevices * CL->numPlatforms, sizeof(cl_uint));
	for (int i = 0; i < xdim * ydim * 2 * CL->numDevices; i++)
		h_seeds[i] = rand();


	//TEXTURES
	cl_int tex_size = 0;
	for (int i = 0; i < s->mat_count; i++)
		if (s->materials[i].map_Kd)
			tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;

	cl_uchar *h_tex = calloc(sizeof(cl_uchar), tex_size);
	tex_size = 0;

	gpu_mat *simple_mats = calloc(s->mat_count, sizeof(gpu_mat));
	for (int i = 0; i < s->mat_count; i++)
	{
		if (s->materials[i].map_Kd)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_Kd->pixels, s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3);
			simple_mats[i] = (gpu_mat){
				s->materials[i].Ka,
				s->materials[i].Kd,
				s->materials[i].Ks,
				s->materials[i].Ke,
				tex_size,
				s->materials[i].map_Kd->width,
				s->materials[i].map_Kd->height
			};
			tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;
		}
		else
		{
			printf("tex for material %d is null!\n", i);
			simple_mats[i] = (gpu_mat){
				s->materials[i].Ka,
				s->materials[i].Kd,
				s->materials[i].Ks,
				s->materials[i].Ke,
				0,
				0,
				0,
			};
		}
	}

	//BINS
	gpu_bin *flat_bvh = flatten_bvh(s->bins, s->bin_count);

	//COMBINE
	gpu_scene *gs = calloc(1, sizeof(gpu_scene));
	*gs = (gpu_scene){V, T, N, s->face_count * 3, flat_bvh, s->bin_count, h_tex, tex_size, simple_mats, s->mat_count, h_seeds, xdim * ydim * 2 * CL->numDevices * CL->numPlatforms};
	return gs;
}

gpu_context *prep_gpu(void)
{
	printf("prepping for GPU launch\n");
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
        err = clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_ALL, 0, NULL, &d);
        if (err == CL_SUCCESS)
            gpu->numDevices += d;
    }

    printf("%u devices, %u platforms\n", gpu->numDevices, gpu->numPlatforms);

    //get ids for devices and create (platforms) compute contexts, (devices) command queues
    cl_device_id device_ids[gpu->numDevices];
    cl_uint offset = 0;
    gpu->contexts = calloc(gpu->numPlatforms, sizeof(cl_context));
    gpu->commands = calloc(gpu->numDevices, sizeof(cl_command_queue));
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	cl_uint d;
    	clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_GPU, gpu->numDevices, &device_ids[offset], &d);
    	gpu->contexts[i] = clCreateContext(0, d, &device_ids[offset], NULL, NULL, &err);
    	for (int j = 0; j < d; j++)
    		gpu->commands[offset + j] = clCreateCommandQueue(gpu->contexts[i], device_ids[offset + j], CL_QUEUE_PROFILING_ENABLE, &err);
    	offset += d;
    }
    printf("made contexts and CQs\n");

    char *source = load_cl_file("new_kernel.cl");


    //create (platforms) programs and build them
    gpu->programs = calloc(gpu->numPlatforms, sizeof(cl_program));
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	gpu->programs[i] = clCreateProgramWithSource(gpu->contexts[i], 1, (const char **) &source, NULL, &err);
    	err = clBuildProgram(gpu->programs[i], 0, NULL, NULL, NULL, NULL);
    	if (err != CL_SUCCESS)
    	{
    	 	printf("bad compile\n");
    	 	exit(0);
    	}
	    free(source);
    }
    return gpu;
}

cl_double3 *composite(cl_float3 **outputs, int numDevices, int resolution)
{
	cl_double3 *output_sum = calloc(resolution, sizeof(cl_double3));
	for (int i = 0; i < numDevices; i++)
	{
		for (int j = 0; j < resolution; j++)
		{
			output_sum[j].x += (double)outputs[i][j].x;
			output_sum[j].y += (double)outputs[i][j].y;
			output_sum[j].z += (double)outputs[i][j].z;
		}
		free(outputs[i]);
	}

	//average samples and apply tone mapping
	double Lw = 0.0;
	for (int j = 0;j < resolution; j++)
	{
		double scale = 1.0 / (double)(SAMPLES_PER_DEVICE * numDevices);
		output_sum[j].x *= scale;
		output_sum[j].y *= scale;
		output_sum[j].z *= scale;

		Lw += log(0.1 + 0.2126 * output_sum[j].x + 0.7152 * output_sum[j].y + 0.0722 * output_sum[j].z);
	}
	printf("Lw is %lf\n", Lw);
	

	Lw /= (double)resolution;
	printf("Lw is %lf\n", Lw);

	Lw = exp(Lw);
	printf("Lw is %lf\n", Lw);

	for (int j = 0; j < resolution; j++)
	{
		output_sum[j].x = output_sum[j].x * 0.36 / Lw;
		output_sum[j].y = output_sum[j].y * 0.36 / Lw;
		output_sum[j].z = output_sum[j].z * 0.36 / Lw;

		output_sum[j].x = output_sum[j].x / (output_sum[j].x + 1.0);
		output_sum[j].y = output_sum[j].y / (output_sum[j].y + 1.0);
		output_sum[j].z = output_sum[j].z / (output_sum[j].z + 1.0);
	}
	return output_sum;
}

cl_double3 *gpu_render(Scene *S, t_camera cam, int xdim, int ydim)
{
	gpu_context *CL = prep_gpu();
	gpu_scene *scene = prep_scene(S, CL, xdim, ydim);

	//for simplicity assuming one platform for now. can easily be extended, see old gpu_launch.c

	//per-platform pointers
	cl_mem d_V;
	cl_mem d_T;
	cl_mem d_N;
	cl_mem d_bins;
	cl_mem d_mats;
	cl_mem d_tex;
	
	//per-platform allocs
	d_V = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY, sizeof(cl_float3) * scene->tri_count, NULL, NULL);
	d_T = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY, sizeof(cl_float3) * scene->tri_count, NULL, NULL);
	d_N = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY, sizeof(cl_float3) * scene->tri_count, NULL, NULL);
	d_mats = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY, sizeof(gpu_mat) * scene->mat_count, NULL, NULL);
	d_bins = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY, sizeof(gpu_bin) * scene->bin_count, NULL, NULL);
	d_tex = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY, sizeof(cl_uchar) * scene->tex_size, NULL, NULL);

	//per-platform copies
	clEnqueueWriteBuffer(CL->commands[0], d_V, CL_TRUE, 0, sizeof(cl_float3) * scene->tri_count, scene->V, 0, NULL, NULL);
	clEnqueueWriteBuffer(CL->commands[0], d_N, CL_TRUE, 0, sizeof(cl_float3) * scene->tri_count, scene->N, 0, NULL, NULL);
	clEnqueueWriteBuffer(CL->commands[0], d_T, CL_TRUE, 0, sizeof(cl_float3) * scene->tri_count, scene->T, 0, NULL, NULL);
	clEnqueueWriteBuffer(CL->commands[0], d_mats, CL_TRUE, 0, sizeof(gpu_mat) * scene->mat_count, scene->mats, 0, NULL, NULL);
	clEnqueueWriteBuffer(CL->commands[0], d_bins, CL_TRUE, 0, sizeof(gpu_bin) * scene->bin_count, scene->bins, 0, NULL, NULL);
	clEnqueueWriteBuffer(CL->commands[0], d_tex, CL_TRUE, 0, sizeof(cl_uchar) * scene->tex_size, scene->tex, 0, NULL, NULL);

	//per-device pointers
	cl_mem *d_outputs;
	cl_mem *d_seeds;
	d_outputs = calloc(CL->numDevices, sizeof(cl_mem));
	d_seeds = calloc(CL->numDevices, sizeof(cl_mem));
	
	cl_uint d;
	clGetDeviceIDs(CL->platform[0], CL_DEVICE_TYPE_ALL, 0, NULL, &d);

	size_t resolution = xdim * ydim;
	size_t groupsize = 256;
	size_t samples = SAMPLES_PER_DEVICE;

	//per-device allocs and copies
	for (int i = 0; i < d; i++)
	{
		d_seeds[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY, sizeof(cl_uint) * 2 * resolution, NULL, NULL);
		clEnqueueWriteBuffer(CL->commands[i], d_seeds[i], CL_TRUE, 0, sizeof(cl_uint) * 2 * resolution, &scene->seeds[2 * resolution * i], 0, NULL, NULL);
		d_outputs[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE, sizeof(cl_float3) * resolution, NULL, NULL);
	}


	cl_kernel render = clCreateKernel(CL->programs[0], "render_kernel", NULL);

	//per-platform args
	clSetKernelArg(render, 0, sizeof(cl_mem), &d_V);
	clSetKernelArg(render, 1, sizeof(cl_mem), &d_T);
	clSetKernelArg(render, 2, sizeof(cl_mem), &d_N);
	clSetKernelArg(render, 4, sizeof(cl_mem), &d_bins);
	clSetKernelArg(render, 5, sizeof(cl_mem), &d_mats);
	clSetKernelArg(render, 6, sizeof(cl_mem), &d_tex);
	clSetKernelArg(render, 7, sizeof(cl_float3), &cam.origin);
	clSetKernelArg(render, 8, sizeof(cl_float3), &cam.focus);
	clSetKernelArg(render, 9, sizeof(cl_float3), &cam.d_x);
	clSetKernelArg(render, 10, sizeof(cl_float3), &cam.d_y);
	clSetKernelArg(render, 11, sizeof(cl_uint), &samples);
	clSetKernelArg(render, 12, sizeof(cl_uint), &xdim);

	//per-device args and launch
	cl_event done[CL->numDevices];
	cl_float3 **outputs = calloc(CL->numDevices, sizeof(cl_float3 *));
	
	for (int i = 0; i < d; i++)
	{
		clSetKernelArg(render, 13, sizeof(cl_mem), &d_seeds[i]);
		clSetKernelArg(render, 14, sizeof(cl_mem), &d_outputs[i]);
		outputs[i] = calloc(resolution, sizeof(cl_float3));

		clEnqueueNDRangeKernel(CL->commands[i], render, 1, 0, &resolution, &groupsize, 0, 0, &done[i]);
		clEnqueueReadBuffer(CL->commands[i], d_outputs[i], CL_FALSE, 0, sizeof(cl_float3) * resolution, outputs[i], 1, &done[i], NULL);
	}

	for (int i = 0; i < d; i++)
		clFlush(CL->commands[i]);

	printf("all enqueued\n");
	for (int i = 0; i < d; i++)
	{
		clFinish(CL->commands[i]);
		cl_ulong start, end;
		clGetEventProfilingInfo(done[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
		clGetEventProfilingInfo(done[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
		printf("device %d took %.3f seconds\n", i, (float)(end - start) / 1000000000.0f);
	}
	printf("done?\n");

	return composite(outputs, CL->numDevices, resolution);
}