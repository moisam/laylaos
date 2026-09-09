
#include <gui/gui.h>
#include <gui/bitmap.h>
#include <gui/client/window.h>
#include "egl_dri2.h"
#include "eglglobals.h"

static void
laylaos_put_image2(struct dri_drawable *draw, int op, int x, int y, int w,
                   int h, int stride, char *data, void *loaderPrivate)
{
    fprintf(stderr, "%s:\n", __func__);
}

static void
laylaos_put_image(struct dri_drawable *drawable, int op,
                  int x, int y, int width, int height,
                  char *data, void *loaderPrivate)
{
    struct dri2_egl_surface *dri2_surf = (struct dri2_egl_surface *)loaderPrivate;

    if (dri2_surf->base.Type == EGL_PBUFFER_BIT || !dri2_surf->window) {
        return;
    }

    struct window_t *lw = dri2_surf->window;
    struct gc_t gc;
    struct screen_t screen;

    gc.clipping.clip_rects = NULL;
    gc.clipping.clipping_on = 0;

    gc.w = width;
    gc.h = height;
    gc.pixel_width = 4;
    gc.buffer = (uint8_t *)data;
    gc.buffer_size = width * height * 4;
    gc.pitch = width * 4;
    gc.screen = &screen;

    screen.pixel_width = 4;
    screen.red_pos = 16;
    screen.green_pos = 8;
    screen.blue_pos = 0;
    screen.rgb_mode = 1;

    gc_copy_part_gc(lw->gc, &gc, x, y, 0, 0, width, height, 0);

#if 0
    struct window_t *lw = dri2_surf->window;

    struct bitmap32_t bitmap;
    volatile uint32_t *p, *lp;

    bitmap.width = width;
    bitmap.height = height;
    bitmap.data = (uint32_t *)data;
    lp = (uint32_t *)(data + (width * height * sizeof(uint32_t)));

    for(p = (uint32_t *)data; p < lp; p++)
    {
        *p = (*p << 8) | 0xff /* ((*p >> 24) & 0xff) */;
    }

    gc_blit_bitmap(lw->gc, &bitmap, x, y, 0, 0, width, height);
#endif

    window_invalidate(lw);
}

static void
laylaos_get_image(struct dri_drawable *drawable,
                  int x, int y, int width, int height,
                  char *data, void *loaderPrivate)
{
    fprintf(stderr, "%s:\n", __func__);
    /* Optional: Only needed if Mesa needs to read pixels back from screen */
}

static void
laylaos_get_drawable_info(struct dri_drawable *drawable,
                          int *x, int *y, int *width, int *height,
                          void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = (struct dri2_egl_surface *)loaderPrivate;

   *x = 0;
   *y = 0;

   if (dri2_surf->base.Type == EGL_WINDOW_BIT && dri2_surf->window) {
      struct window_t *lw = dri2_surf->window;
      *width = lw->w;
      *height = lw->h;
   } else if (dri2_surf->base.Type == EGL_PBUFFER_BIT) {
      // Pbuffer dimensions are stored explicitly in the base EGL surface structure
      *width = dri2_surf->base.Width;
      *height = dri2_surf->base.Height;
   } else {
      *width = 0;
      *height = 0;
   }
}

static _EGLSurface *
laylaos_create_window_surface(_EGLDisplay *disp, _EGLConfig *conf,
                              void *native_window, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf = calloc(1, sizeof(*dri2_surf));
   const struct dri_config *config;

   if (!dri2_surf) return NULL;

   /* 1. Initialize the base EGL surface */
   if (!dri2_init_surface(&dri2_surf->base, disp, EGL_WINDOW_BIT, conf, attrib_list, true,
                          native_window))
      goto cleanup;

   config = dri2_get_dri_config(dri2_conf, EGL_WINDOW_BIT, dri2_surf->base.GLColorspace);
   if (!config) {
      _eglError(EGL_BAD_MATCH,
                "Unsupported surfacetype/colorspace configuration");
      goto cleanup;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup;

   if (native_window)
      dri2_surf->window = native_window;

   return &dri2_surf->base;

cleanup:
   free(dri2_surf);
   return NULL;
}

static _EGLSurface *
laylaos_create_pbuffer_surface(_EGLDisplay *disp, _EGLConfig *conf,
                               const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf = calloc(1, sizeof(*dri2_surf));
   const struct dri_config *config;

   if (!dri2_surf) return NULL;

   if (!dri2_init_surface(&dri2_surf->base, disp, EGL_PBUFFER_BIT, conf, attrib_list, false, NULL))
      goto cleanup;

   config = dri2_get_dri_config(dri2_conf, EGL_PBUFFER_BIT, dri2_surf->base.GLColorspace);
   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup;

   dri2_surf->window = NULL; 

   return &dri2_surf->base;

cleanup:
   free(dri2_surf);
   return NULL;
}

static EGLBoolean
laylaos_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   driDestroyDrawable(dri2_surf->dri_drawable);

   dri2_fini_surface(surf);
   free(dri2_surf);

   return EGL_TRUE;
}

static EGLBoolean
laylaos_swap_interval(_EGLDisplay *disp, _EGLSurface *surf, EGLint interval)
{
   surf->SwapInterval = 1;
   return EGL_TRUE;
}

static EGLint
laylaos_query_buffer_age(_EGLDisplay *disp, _EGLSurface *surface)
{
   fprintf(stderr, "%s:\n", __func__);
   return 0;
}

static EGLBoolean
laylaos_swap_buffers(_EGLDisplay *disp, _EGLSurface *draw)
{
   //struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);

   driSwapBuffers(dri2_surf->dri_drawable);

   return EGL_TRUE;
}

static EGLBoolean
laylaos_query_surface(_EGLDisplay *disp, _EGLSurface *surf, EGLint attribute,
                      EGLint *value)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   switch (attribute) {
   case EGL_WIDTH:
      if (dri2_surf->base.Type == EGL_WINDOW_BIT && dri2_surf->window) {
         struct window_t *lw = dri2_surf->window;
         *value = lw->w;
         return EGL_TRUE;
      }
      break;
   case EGL_HEIGHT:
      if (dri2_surf->base.Type == EGL_WINDOW_BIT && dri2_surf->window) {
         struct window_t *lw = dri2_surf->window;
         *value = lw->h;
         return EGL_TRUE;
      }
      break;
   default:
      break;
   }
   return _eglQuerySurface(disp, surf, attribute, value);
}


static const __DRIswrastLoaderExtension laylaos_swrast_loader_extension = {
   .base = {__DRI_SWRAST_LOADER, 2},

   .getDrawableInfo = laylaos_get_drawable_info,
   .putImage = laylaos_put_image,
   .getImage = laylaos_get_image,
   .putImage2 = laylaos_put_image2,
};

static const __DRIextension *laylaos_loader_extensions[] = {
    &laylaos_swrast_loader_extension.base,
    NULL
};

static const struct dri2_egl_display_vtbl laylaos_display_vtbl = {
   .authenticate = NULL,
   .create_window_surface = laylaos_create_window_surface,
   .create_pbuffer_surface = laylaos_create_pbuffer_surface,
   .destroy_surface = laylaos_destroy_surface,
   .create_image = NULL,
   .swap_buffers = laylaos_swap_buffers,
   .swap_interval = laylaos_swap_interval,
   .query_buffer_age = laylaos_query_buffer_age,
   .query_surface = laylaos_query_surface,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
   .set_shared_buffer_mode = NULL,
};


static void
laylaos_add_configs_for_visuals(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   int count = 0;

   for (int j = 0; dri2_dpy->driver_configs[j]; j++) {
      const struct gl_config *gl_config =
            (struct gl_config *) dri2_dpy->driver_configs[j];

      enum pipe_format linear_format =
            util_format_linear(gl_config->color_format);

      //fprintf(stderr, "%s: format %x (%x, %x)\n", __func__, linear_format, PIPE_FORMAT_B8G8R8A8_UNORM, PIPE_FORMAT_R8G8B8A8_UNORM);

      if (linear_format != PIPE_FORMAT_B8G8R8X8_UNORM)
            continue;

      const EGLint surface_type = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
      const EGLint attr_list[] = {
         EGL_NATIVE_VISUAL_ID, 1,
         EGL_NONE,
      };

      struct dri2_egl_config *dri2_conf = dri2_add_config(
            disp, dri2_dpy->driver_configs[j], surface_type, attr_list);

      if (dri2_conf)
            count++;
   }

   if(!count)
      fprintf(stderr, "No DRI config supports native RGBA format\n");
}

EGLBoolean
dri2_initialize_laylaos(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   char *dummy_argv[] = { "egl", NULL };
   gui_init(1, dummy_argv);

   dri2_dpy->driver_name = strdup("swrast");
   dri2_dpy->loader_extensions = laylaos_loader_extensions;
   dri2_dpy->fd_render_gpu = -1;
   dri2_detect_swrast_kopper(disp);

   if (!dri2_create_screen(disp)) {
       fprintf(stderr, "DRI2: Failed to create swrast screen\n");
       return EGL_FALSE;
   }

   dri2_setup_screen(disp);
   dri2_setup_swap_interval(disp, 1);

   /* Create configs *after* enabling extensions because presence of DRI
    * driver extensions can affect the capabilities of EGLConfigs.
    */
   laylaos_add_configs_for_visuals(disp);

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &laylaos_display_vtbl;

   return EGL_TRUE;
}

void
dri2_teardown_laylaos(struct dri2_egl_display *dri2_dpy)
{
}

