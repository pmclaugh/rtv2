#include "rt.h"

void	draw_pixels(t_sdl *sdl)
{
	SDL_LockSurface(sdl->screen);
	Uint32 *pixels = (Uint32*)sdl->screen->pixels;
	for (int y = 0; y < sdl->screen->h; y++)
		for (int x = 0; x < sdl->screen->w; x++)
		{
			pixels[x + (y * sdl->screen->w)] = SDL_MapRGB(sdl->screen->format, sdl->pixels[x + (y * sdl->screen->w)].x * 255, sdl->pixels[x + (y * sdl->screen->w)].y * 255, sdl->pixels[x + (y * sdl->screen->w)].z * 255);
		}
	SDL_UnlockSurface(sdl->screen);
	SDL_UpdateWindowSurface(sdl->win);

	// SDL_Renderer	*renderer = SDL_GetRenderer(sdl->win);
	// // SDL_Renderer	*renderer = SDL_CreateRenderer(sdl->win, -1, SDL_RENDERER_ACCELERATED);
	// // SDL_SetRenderDrawColor(renderer, 255, 0, 0, 1);
	// SDL_Surface		*surface = SDL_LoadBMP("Smiley.bmp");

	// SDL_Texture		*texture = SDL_CreateTextureFromSurface(renderer, surface);

	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(renderer, &info);
	// // printf("%d\t%d\n", info.max_texture_width, info.max_texture_height);
	// printf("%s\n", info.name);

	// // SDL_Rect rect;
	// // rect.x = 0;
	// // rect.y = 0;
	// // rect.w = 100;
	// // rect.h = 100;

	// SDL_RenderClear(renderer);
	// SDL_RenderCopy(renderer, texture, NULL, NULL);
	// SDL_RenderPresent(renderer);
}

void		init_sdl_pt(t_env *env)
{
	env->pt = malloc(sizeof(t_sdl));
	env->pt->win = SDL_CreateWindow("CLIVE - Path Tracer", 900, 500, DIM_PT, DIM_PT, SDL_WINDOW_INPUT_FOCUS);
	env->pt->screen = SDL_GetWindowSurface(env->pt->win);
	SDL_SetSurfaceRLE(env->pt->screen, 1);
	env->pt->pixels = calloc(sizeof(cl_float3), (DIM_PT * DIM_PT));
	env->pt->total_clr = calloc(sizeof(cl_float3), (DIM_PT * DIM_PT));
	env->pt->renderer = SDL_GetRenderer(env->pt->win);
}

void		init_sdl_ia(t_env *env)
{
	env->ia = malloc(sizeof(t_sdl));
	env->ia->win = SDL_CreateWindow("CLIVE - Interactive Mode", 500, 500, DIM_IA, DIM_IA, SDL_WINDOW_INPUT_FOCUS);
	env->ia->screen = SDL_GetWindowSurface(env->ia->win);
	SDL_SetSurfaceRLE(env->ia->screen, 1);
	env->ia->pixels = calloc(sizeof(cl_float3), (DIM_IA * DIM_IA));
	env->ia->total_clr = NULL;
	env->ia->renderer = SDL_GetRenderer(env->ia->win);
	// env->ia->renderer = SDL_CreateRenderer(env->ia->win, -1, SDL_RENDERER_ACCELERATED);
	SDL_RendererInfo info;
	SDL_GetRendererInfo(env->ia->renderer, &info);
	// printf("%d\t%d\n", info.max_texture_width, info.max_texture_height);
	// printf("%s\n", info.name);
	// printf("%u\n", info.flags);
}

void		run_sdl(t_env *env)
{
	SDL_Init(SDL_INIT_VIDEO);
	if (env->mode == IA)
		init_sdl_ia(env);

	while (env->running) //MAIN LOOP
	{
		env->previous_tick = env->current_tick;
		env->current_tick = SDL_GetTicks();
		while (SDL_PollEvent(&env->event))
		{
			if (env->event.type == SDL_QUIT)
				exit_hook(env);
			else if (env->event.type == SDL_KEYDOWN)
				key_press(env->event.key.keysym.sym, env);
			else if (env->event.type == SDL_KEYUP)
				key_release(env->event.key.keysym.sym, env);
			else if (env->event.type == SDL_MOUSEBUTTONDOWN)
				mouse_press(env->event.button.x, env->event.button.y, env);
		}
		handle_input(env);
	}
}