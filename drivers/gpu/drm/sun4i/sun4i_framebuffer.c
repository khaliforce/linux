// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2015 Free Electrons
 * Copyright (C) 2015 NextThing Co
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include "sun4i_drv.h"
#include "sun4i_framebuffer.h"

/*
 * Write back the CPU cache lines of the changed rows. The display engine reads RAM.
 * X reports the changed area after it updates the screen. Whole rows are cleaned,
 * which keeps the number of cache maintenance calls low.
 */
static int sun4i_fb_dirty(struct drm_framebuffer *fb, struct drm_file *file,
			  unsigned int flags, unsigned int color,
			  struct drm_clip_rect *clips, unsigned int num_clips)
{
	struct drm_gem_dma_object *obj = drm_fb_dma_get_gem_obj(fb, 0);
	unsigned int pitch = fb->pitches[0];
	unsigned int i, y1, y2;

	if (!obj->map_noncoherent)
		return 0;

	if (!clips || !num_clips) {
		dma_sync_single_for_device(fb->dev->dev, obj->dma_addr + fb->offsets[0],
					   (size_t)pitch * fb->height, DMA_TO_DEVICE);
		return 0;
	}

	for (i = 0; i < num_clips; i++) {
		y1 = min_t(unsigned int, clips[i].y1, fb->height);
		y2 = min_t(unsigned int, clips[i].y2, fb->height);
		if (y1 >= y2)
			continue;
		dma_sync_single_for_device(fb->dev->dev, obj->dma_addr + fb->offsets[0] + (size_t)y1 * pitch,
					   (size_t)(y2 - y1) * pitch, DMA_TO_DEVICE);
	}

	return 0;
}

static const struct drm_framebuffer_funcs sun4i_fb_funcs = {
	.create_handle	= drm_gem_fb_create_handle,
	.destroy	= drm_gem_fb_destroy,
	.dirty		= sun4i_fb_dirty,
};

static struct drm_framebuffer *sun4i_fb_create(struct drm_device *drm, struct drm_file *file,
					       const struct drm_format_info *info,
					       const struct drm_mode_fb_cmd2 *mode_cmd)
{
	if (!sun4i_cached_fb)
		return drm_gem_fb_create(drm, file, info, mode_cmd);

	return drm_gem_fb_create_with_funcs(drm, file, info, mode_cmd, &sun4i_fb_funcs);
}

static const struct drm_mode_config_funcs sun4i_de_mode_config_funcs = {
	.atomic_check		= drm_atomic_helper_check,
	.atomic_commit		= drm_atomic_helper_commit,
	.fb_create		= sun4i_fb_create,
};

static const struct drm_mode_config_helper_funcs sun4i_de_mode_config_helpers = {
	.atomic_commit_tail	= drm_atomic_helper_commit_tail_rpm,
};

void sun4i_framebuffer_init(struct drm_device *drm)
{
	drm_mode_config_reset(drm);

	drm->mode_config.max_width = 8192;
	drm->mode_config.max_height = 8192;
	drm->mode_config.normalize_zpos = true;

	drm->mode_config.funcs = &sun4i_de_mode_config_funcs;
	drm->mode_config.helper_private = &sun4i_de_mode_config_helpers;
}
