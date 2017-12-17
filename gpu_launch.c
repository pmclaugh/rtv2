


cl_float3 *gpu_render(Scene *s, t_camera cam, int xdim, int ydim)
{
	CL *gpu = prep_gpu();
	gpu_scene *scene = prep_scene(s);

    printf("built programs\n");




	//MEMORY ALLOC
	cl_mem *d_V;
	cl_mem *d_T;
	cl_mem *d_N;
	cl_mem *d_boxes;
	cl_mem **d_output;
	cl_mem **d_seeds;
	cl_mem *d_mats;
	cl_mem *d_tex;

	

	int offset = 0;
	d_V = calloc(gpu->numPlatforms, sizeof(cl_mem));
	d_T = calloc(gpu->numPlatforms, sizeof(cl_mem));
	d_N = calloc(gpu->numPlatforms, sizeof(cl_mem));
	d_mats = calloc(gpu->numPlatforms, sizeof(cl_mem));
	d_output = calloc(gpu->numPlatforms, sizeof(cl_mem *));
	d_seeds = calloc(gpu->numPlatforms, sizeof(cl_mem *));
	d_boxes = calloc(gpu->numPlatforms, sizeof(cl_mem));
	d_tex = calloc(gpu->numPlatforms, sizeof(cl_mem));
	for (int i = 0; i < gpu->numPlatforms; i++)
	{
		cl_uint d;
		clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_ALL, 0, NULL, &d);

		d_output[i] = calloc(d, sizeof(cl_mem));
		d_seeds[i] = calloc(d, sizeof(cl_mem));
		
		d_mats[i] = clCreateBuffer(gpu->contexts[i], CL_MEM_READ_ONLY, sizeof(gpu_mat) * s->mat_count, NULL, NULL);
		d_boxes[i] = clCreateBuffer(gpu->contexts[i], CL_MEM_READ_ONLY, sizeof(Box) * s->box_count, NULL, NULL);
		d_tex[i] = clCreateBuffer(gpu->contexts[i], CL_MEM_READ_ONLY, sizeof(cl_uchar) * tex_size, NULL, NULL);
		
		for (int j = 0; j < d; j++)
		{
			d_output[i][j] = clCreateBuffer(gpu->contexts[i], CL_MEM_WRITE_ONLY, sizeof(cl_float3) * xdim * ydim, NULL, NULL);
			d_seeds[i][j] = clCreateBuffer(gpu->contexts[i], CL_MEM_READ_ONLY, sizeof(cl_uint) * xdim * ydim * 2, NULL, NULL);
			clEnqueueWriteBuffer(gpu->commands[offset + j], d_scene[i], CL_TRUE, 0, sizeof(Face) * s->face_count, s->faces, 0, NULL, NULL);
			clEnqueueWriteBuffer(gpu->commands[offset + j], d_mats[i], CL_TRUE, 0, sizeof(gpu_mat) * s->mat_count, simple_mats, 0, NULL, NULL);
			clEnqueueWriteBuffer(gpu->commands[offset + j], d_seeds[i][offset + j], CL_TRUE, 0, sizeof(cl_uint) * xdim * ydim * 2, &h_seeds[(offset + j) * xdim * ydim * 2], 0, NULL, NULL);
			clEnqueueWriteBuffer(gpu->commands[offset + j], d_boxes[i], CL_TRUE, 0, sizeof(Box) * s->box_count, s->boxes, 0, NULL, NULL);
			clEnqueueWriteBuffer(gpu->commands[offset + j], d_const_boxes[i], CL_TRUE, 0, sizeof(C_Box) * s->c_box_count, s->c_boxes, 0, NULL, NULL);
			clEnqueueWriteBuffer(gpu->commands[offset + j], d_tex[i], CL_TRUE, 0, sizeof(cl_uchar) * tex_size, h_tex, 0, NULL, NULL);
		}
		offset += d;
	}

	// Create the compute kernel from the program (REALLY need to clean all this up before there's multiple kernels)
	cl_kernel *kernels = calloc(gpu->numPlatforms, sizeof(cl_kernel));
	for (int i = 0; i < gpu->numPlatforms; i++)
		kernels[i] = clCreateKernel(gpu->programs[i], "render_kernel", NULL);

	size_t resolution = xdim * ydim;
	size_t groupsize = 256;
	cl_int obj_count = s->c_box_count;

	cl_uint samples_per_device = SAMPLES_PER_DEVICE;


	//ARGS AND LAUNCH

	for (int i = 0; i < numPlatforms; i++)
	{
		//clSetKernelArg(kernels[i], 0, sizeof(cl_mem), &d_scene[i]);
		clSetKernelArg(kernels[i], 1, sizeof(cl_mem), &d_mats[i]);
		clSetKernelArg(kernels[i], 2, sizeof(cl_mem), &d_boxes[i]);
		clSetKernelArg(kernels[i], 3, sizeof(cl_float3), &cam.origin);
		clSetKernelArg(kernels[i], 4, sizeof(cl_float3), &cam.focus);
		clSetKernelArg(kernels[i], 5, sizeof(cl_float3), &cam.d_x);
		clSetKernelArg(kernels[i], 6, sizeof(cl_float3), &cam.d_y);
		clSetKernelArg(kernels[i], 7, sizeof(cl_int), &xdim);
		// clSetKernelArg(kernels[i], 8, sizeof(cl_mem), &d_output[i]);
		// clSetKernelArg(kernels[i], 9, sizeof(cl_mem), &d_seeds[i]);
		clSetKernelArg(kernels[i], 10, sizeof(cl_uint), &samples_per_device);
		clSetKernelArg(kernels[i], 11, sizeof(cl_mem), &d_const_boxes[i]);
		clSetKernelArg(kernels[i], 12, sizeof(cl_uint), &obj_count);
		clSetKernelArg(kernels[i], 13, sizeof(cl_mem), &d_tex[i]);
	}

	printf("about to fire\n");
	//pull the trigger

	cl_event render[numDevices];

	cl_float3 **outputs = calloc(gpu->numDevices, sizeof(cl_float3 *));
	cl_uint offset = 0;
	for (int i = 0; i < gpu->numPlatforms; i++)
	{
		cl_uint d;
		clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_ALL, 0, NULL, &d);
		for (int j = 0; j < d; j++)
		{
			clSetKernelArg(kernels[i], 8, sizeof(cl_mem), &d_output[i][offset + j]);
			clSetKernelArg(kernels[i], 9, sizeof(cl_mem), &d_seeds[i][offset + j]);
			outputs[offset + j] = calloc(xdim * ydim, sizeof(cl_float3));
			cl_int err = clEnqueueNDRangeKernel(gpu->commands[offset + j], kernels[i], 1, 0, &resolution, &groupsize, 0, NULL, &render[offset + j]);
			clEnqueueReadBuffer(gpu->commands[offset + j], d_output[i][j], CL_FALSE, 0, sizeof(cl_float3) * xdim * ydim, outputs[offset + j], 1, &render[offset + j], NULL);
		}
		offset += d;
	}

	for (int i = 0; i < gpu->numDevices; i++)
		clFlush(gpu->commands[i]);

	printf("all enqueued\n");
	for (int i = 0; i < gpu->numDevices; i++)
	{
		clFinish(commands[i]);
		cl_ulong start, end;
		clGetEventProfilingInfo(render[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
		clGetEventProfilingInfo(render[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
		printf("kernel %d took %.3f seconds\n", i, (float)(end - start) / 1000000000.0f);
	}
	printf("done?\n");



	//TONE MAPPING (SHOULD BE SEPARATE FUNCTION)


	//sum samples. do all transformations and sums in doubles because values may be quite different sizes
	//I am not sure if this is necessary but it can't hurt.
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
		double scale = 1.0 / (double)(samples_per_device * numDevices);
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

	//just a hack til i refactor stuff later down the pipeline to take doubles.
	cl_float3 *float_sum = calloc(resolution, sizeof(cl_float3));
	for (int j = 0; j < resolution; j++)
	{
		float_sum[j].x = (float)output_sum[j].x;
		float_sum[j].y = (float)output_sum[j].y;
		float_sum[j].z = (float)output_sum[j].z;
	}

	free(kernels);
	free(output_sum);
	return float_sum;
}