/*
 * fbsysfs.c - framebuffer device class and attributes
 *
 * Copyright (c) 2004 James Simmons <jsimmons@infradead.org>
 * 
 *	This program is free software you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

/*
 * Note:  currently there's only stubs for framebuffer_alloc and
 * framebuffer_release here.  The reson for that is that until all drivers
 * are converted to use it a sysfsification will open OOPSable races.
 */

#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/console.h>

/**
 * framebuffer_alloc - creates a new frame buffer info structure
 *
 * @size: size of driver private data, can be zero
 * @dev: pointer to the device for this fb, this can be NULL
 *
 * Creates a new frame buffer info structure. Also reserves @size bytes
 * for driver private data (info->par). info->par (if any) will be
 * aligned to sizeof(long).
 *
 * Returns the new structure, or NULL if an error occured.
 *
 */
struct fb_info *framebuffer_alloc(size_t size, struct device *dev)
{
#define BYTES_PER_LONG (BITS_PER_LONG/8)
#define PADDING (BYTES_PER_LONG - (sizeof(struct fb_info) % BYTES_PER_LONG))
	int fb_info_size = sizeof(struct fb_info);
	struct fb_info *info;
	char *p;

	if (size)
		fb_info_size += PADDING;

	p = kmalloc(fb_info_size + size, GFP_KERNEL);
	if (!p)
		return NULL;
	memset(p, 0, fb_info_size + size);
	info = (struct fb_info *) p;

	if (size)
		info->par = p + fb_info_size;

	info->device = dev;

	return info;
#undef PADDING
#undef BYTES_PER_LONG
}
EXPORT_SYMBOL(framebuffer_alloc);

/**
 * framebuffer_release - marks the structure available for freeing
 *
 * @info: frame buffer info structure
 *
 * Drop the reference count of the class_device embedded in the
 * framebuffer info structure.
 *
 */
void framebuffer_release(struct fb_info *info)
{
	kfree(info);
}
EXPORT_SYMBOL(framebuffer_release);

static int activate(struct fb_info *fb_info, struct fb_var_screeninfo *var)
{
	int err;

	var->activate |= FB_ACTIVATE_FORCE;
	acquire_console_sem();
	fb_info->flags |= FBINFO_MISC_USEREVENT;
	err = fb_set_var(fb_info, var);
	fb_info->flags &= ~FBINFO_MISC_USEREVENT;
	release_console_sem();
	if (err)
		return err;
	return 0;
}

static int mode_string(char *buf, unsigned int offset,
		       const struct fb_videomode *mode)
{
	char m = 'U';
	if (mode->flag & FB_MODE_IS_DETAILED)
		m = 'D';
	if (mode->flag & FB_MODE_IS_VESA)
		m = 'V';
	if (mode->flag & FB_MODE_IS_STANDARD)
		m = 'S';
	return snprintf(&buf[offset], PAGE_SIZE - offset, "%c:%dx%d-%d\n", m, mode->xres, mode->yres, mode->refresh);
}

static ssize_t store_mode(struct class_device *class_device, const char * buf,
			  size_t count)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	char mstr[100];
	struct fb_var_screeninfo var;
	struct fb_modelist *modelist;
	struct fb_videomode *mode;
	struct list_head *pos;
	size_t i;
	int err;

	memset(&var, 0, sizeof(var));

	list_for_each(pos, &fb_info->modelist) {
		modelist = list_entry(pos, struct fb_modelist, list);
		mode = &modelist->mode;
		i = mode_string(mstr, 0, mode);
		if (strncmp(mstr, buf, max(count, i)) == 0) {

			var = fb_info->var;
			fb_videomode_to_var(&var, mode);
			if ((err = activate(fb_info, &var)))
				return err;
			fb_info->mode = mode;
			return count;
		}
	}
	return -EINVAL;
}

static ssize_t show_mode(struct class_device *class_device, char *buf)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);

	if (!fb_info->mode)
		return 0;

	return mode_string(buf, 0, fb_info->mode);
}

static ssize_t store_modes(struct class_device *class_device, const char * buf,
			   size_t count)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	LIST_HEAD(old_list);
	int i = count / sizeof(struct fb_videomode);

	if (i * sizeof(struct fb_videomode) != count)
		return -EINVAL;

	acquire_console_sem();
	list_splice(&fb_info->modelist, &old_list);
	fb_videomode_to_modelist((struct fb_videomode *)buf, i,
				 &fb_info->modelist);
	if (fb_new_modelist(fb_info)) {
		fb_destroy_modelist(&fb_info->modelist);
		list_splice(&old_list, &fb_info->modelist);
	} else
		fb_destroy_modelist(&old_list);

	release_console_sem();

	return 0;
}

static ssize_t show_modes(struct class_device *class_device, char *buf)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	unsigned int i;
	struct list_head *pos;
	struct fb_modelist *modelist;
	const struct fb_videomode *mode;

	i = 0;
	list_for_each(pos, &fb_info->modelist) {
		modelist = list_entry(pos, struct fb_modelist, list);
		mode = &modelist->mode;
		i += mode_string(buf, i, mode);
	}
	return i;
}

static ssize_t store_bpp(struct class_device *class_device, const char * buf,
			 size_t count)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	struct fb_var_screeninfo var;
	char ** last = NULL;
	int err;

	var = fb_info->var;
	var.bits_per_pixel = simple_strtoul(buf, last, 0);
	if ((err = activate(fb_info, &var)))
		return err;
	return count;
}

static ssize_t show_bpp(struct class_device *class_device, char *buf)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	return snprintf(buf, PAGE_SIZE, "%d\n", fb_info->var.bits_per_pixel);
}

static ssize_t store_virtual(struct class_device *class_device,
			     const char * buf, size_t count)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	struct fb_var_screeninfo var;
	char *last = NULL;
	int err;

	var = fb_info->var;
	var.xres_virtual = simple_strtoul(buf, &last, 0);
	last++;
	if (last - buf >= count)
		return -EINVAL;
	var.yres_virtual = simple_strtoul(last, &last, 0);

	if ((err = activate(fb_info, &var)))
		return err;
	return count;
}

static ssize_t show_virtual(struct class_device *class_device, char *buf)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	return snprintf(buf, PAGE_SIZE, "%d,%d\n", fb_info->var.xres_virtual,
			fb_info->var.yres_virtual);
}

static ssize_t show_stride(struct class_device *class_device, char *buf)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	return snprintf(buf, PAGE_SIZE, "%d\n", fb_info->fix.line_length);
}

/* Format for cmap is "%02x%c%4x%4x%4x\n" */
/* %02x entry %c transp %4x red %4x blue %4x green \n */
/* 256 rows at 16 chars equals 4096, the normal page size */
/* the code will automatically adjust for different page sizes */
static ssize_t store_cmap(struct class_device *class_device, const char *buf,
			  size_t count)
{
	struct fb_info *fb_info = (struct fb_info *)class_get_devdata(class_device);
	int rc, i, start, length, transp = 0;

	if ((count > PAGE_SIZE) || ((count % 16) != 0))
		return -EINVAL;

	if (!fb_info->fbops->fb_setcolreg && !fb_info->fbops->fb_setcmap)
		return -EINVAL;

	sscanf(buf, "%02x", &start);
	length = count / 16;

	for (i = 0; i < length; i++)
		if (buf[i * 16 + 2] != ' ')
			transp = 1;

	/* If we can batch, do it */
	if (fb_info->fbops->fb_setcmap && length > 1) {
		struct fb_cmap umap;

		memset(&umap, 0, sizeof(umap));
		if ((rc = fb_alloc_cmap(&umap, length, transp)))
			return rc;

		umap.start = start;
		for (i = 0; i < length; i++) {
			sscanf(&buf[i * 16 +  3], "%4hx", &umap.red[i]);
			sscanf(&buf[i * 16 +  7], "%4hx", &umap.blue[i]);
			sscanf(&buf[i * 16 + 11], "%4hx", &umap.green[i]);
			if (transp)
				umap.transp[i] = (buf[i * 16 +  2] != ' ');
		}
		rc = fb_info->fbops->fb_setcmap(&umap, fb_info);
		fb_copy_cmap(&umap, &fb_info->cmap);
		fb_dealloc_cmap(&umap);

		return rc;
	}
	for (i = 0; i < length; i++) {
		u16 red, blue, green, tsp;

		sscanf(&buf[i * 16 +  3], "%4hx", &red);
		sscanf(&buf[i * 16 +  7], "%4hx", &blue);
		sscanf(&buf[i * 16 + 11], "%4hx", &green);
		tsp = (buf[i * 16 +  2] != ' ');
		if ((rc = fb_info->fbops->fb_setcolreg(start++,
				      red, green, blue, tsp, fb_info)))
			return rc;

		fb_info->cmap.red[i] = red;
		fb_info->cmap.blue[i] = blue;
		fb_info->cmap.green[i] = green;
		if (transp)
			fb_info->cmap.transp[i] = tsp;
	}
	return 0;
}

static ssize_t show_cmap(struct class_device *class_device, char *buf)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	unsigned int i;

	if (!fb_info->cmap.red || !fb_info->cmap.blue ||
	   !fb_info->cmap.green)
		return -EINVAL;

	if (fb_info->cmap.len > PAGE_SIZE / 16)
		return -EINVAL;

	/* don't mess with the format, the buffer is PAGE_SIZE */
	/* 256 entries at 16 chars per line equals 4096 = PAGE_SIZE */
	for (i = 0; i < fb_info->cmap.len; i++) {
		snprintf(&buf[ i * 16], PAGE_SIZE - i * 16, "%02x%c%4x%4x%4x\n", i + fb_info->cmap.start,
			((fb_info->cmap.transp && fb_info->cmap.transp[i]) ? '*' : ' '),
			fb_info->cmap.red[i], fb_info->cmap.blue[i],
			fb_info->cmap.green[i]);
	}
	return 16 * fb_info->cmap.len;
}

static ssize_t store_blank(struct class_device *class_device, const char * buf,
			   size_t count)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	char *last = NULL;
	int err;

	acquire_console_sem();
	fb_info->flags |= FBINFO_MISC_USEREVENT;
	err = fb_blank(fb_info, simple_strtoul(buf, &last, 0));
	fb_info->flags &= ~FBINFO_MISC_USEREVENT;
	release_console_sem();
	if (err < 0)
		return err;
	return count;
}

static ssize_t show_blank(struct class_device *class_device, char *buf)
{
//	struct fb_info *fb_info = (struct fb_info *)class_get_devdata(class_device);
	return 0;
}

static ssize_t store_console(struct class_device *class_device,
			     const char * buf, size_t count)
{
//	struct fb_info *fb_info = (struct fb_info *)class_get_devdata(class_device);
	return 0;
}

static ssize_t show_console(struct class_device *class_device, char *buf)
{
//	struct fb_info *fb_info = (struct fb_info *)class_get_devdata(class_device);
	return 0;
}

static ssize_t store_cursor(struct class_device *class_device,
			    const char * buf, size_t count)
{
//	struct fb_info *fb_info = (struct fb_info *)class_get_devdata(class_device);
	return 0;
}

static ssize_t show_cursor(struct class_device *class_device, char *buf)
{
//	struct fb_info *fb_info = (struct fb_info *)class_get_devdata(class_device);
	return 0;
}

static ssize_t store_pan(struct class_device *class_device, const char * buf,
			 size_t count)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	struct fb_var_screeninfo var;
	char *last = NULL;
	int err;

	var = fb_info->var;
	var.xoffset = simple_strtoul(buf, &last, 0);
	last++;
	if (last - buf >= count)
		return -EINVAL;
	var.yoffset = simple_strtoul(last, &last, 0);

	acquire_console_sem();
	err = fb_pan_display(fb_info, &var);
	release_console_sem();

	if (err < 0)
		return err;
	return count;
}

static ssize_t show_pan(struct class_device *class_device, char *buf)
{
	struct fb_info *fb_info =
		(struct fb_info *)class_get_devdata(class_device);
	return snprintf(buf, PAGE_SIZE, "%d,%d\n", fb_info->var.xoffset,
			fb_info->var.xoffset);
}

static ssize_t show_name(struct class_device *class_device, char *buf)
{
	struct fb_info *fb_info = (struct fb_info *)class_get_devdata(class_device);

	return snprintf(buf, PAGE_SIZE, "%s\n", fb_info->fix.id);
}

static struct class_device_attribute class_device_attrs[] = {
	__ATTR(bits_per_pixel, S_IRUGO|S_IWUSR, show_bpp, store_bpp),
	__ATTR(blank, S_IRUGO|S_IWUSR, show_blank, store_blank),
	__ATTR(color_map, S_IRUGO|S_IWUSR, show_cmap, store_cmap),
	__ATTR(console, S_IRUGO|S_IWUSR, show_console, store_console),
	__ATTR(cursor, S_IRUGO|S_IWUSR, show_cursor, store_cursor),
	__ATTR(mode, S_IRUGO|S_IWUSR, show_mode, store_mode),
	__ATTR(modes, S_IRUGO|S_IWUSR, show_modes, store_modes),
	__ATTR(pan, S_IRUGO|S_IWUSR, show_pan, store_pan),
	__ATTR(virtual_size, S_IRUGO|S_IWUSR, show_virtual, store_virtual),
	__ATTR(name, S_IRUGO, show_name, NULL),
	__ATTR(stride, S_IRUGO, show_stride, NULL),
};

int fb_init_class_device(struct fb_info *fb_info)
{
	unsigned int i;
	class_set_devdata(fb_info->class_device, fb_info);

	for (i = 0; i < ARRAY_SIZE(class_device_attrs); i++)
		class_device_create_file(fb_info->class_device,
					 &class_device_attrs[i]);
	return 0;
}

void fb_cleanup_class_device(struct fb_info *fb_info)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(class_device_attrs); i++)
		class_device_remove_file(fb_info->class_device,
					 &class_device_attrs[i]);
}


