#include "rt.h"

Scene	*import_file(void)
{
	FILE *fp = fopen("./config.ini", "r");

	if (fp == NULL)
	{
		printf("failed to load config.ini file\n");
		exit(0);
	}

	char *line = calloc(512, 1);
	char *file_path = calloc(512, 1);
	char dir_path[512];
	char *file;
	while (fgets(line, 512, fp))
	{
		if (strncmp(line, "import=", 7) == 0)
			sscanf(line, "import=%s", file_path);
	}
	fclose(fp);

	if ((file = strrchr(file_path, '/') + 1) == NULL)
	{
		printf("path to file in config.ini must begin with \"./\"\n");
		exit(0);
	}
	strncpy(dir_path, file_path, file - file_path);
	
	Scene *S;
	if (strstr(file, ".ply"))
		S = scene_from_ply(dir_path, file);
	else
		S =  scene_from_obj(dir_path, file);
	free(file_path);
	free(line);
	return S;
}
