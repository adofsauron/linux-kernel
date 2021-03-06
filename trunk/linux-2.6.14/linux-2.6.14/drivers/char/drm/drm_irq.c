/**
 * \file drm_irq.h 
 * IRQ support
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Fri Mar 19 14:30:16 1999 by faith@valinux.com
 *
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"

#include <linux/interrupt.h>	/* For task queue support */

/**
 * Get interrupt from bus id.
 * 
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument, pointing to a drm_irq_busid structure.
 * \return zero on success or a negative number on failure.
 * 
 * Finds the PCI device with the specified bus id and gets its IRQ number.
 * This IOCTL is deprecated, and will now return EINVAL for any busid not equal
 * to that of the device that this DRM instance attached to.
 */
int drm_irq_by_busid(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_irq_busid_t __user *argp = (void __user *)arg;
	drm_irq_busid_t p;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
		return -EINVAL;

	if (copy_from_user(&p, argp, sizeof(p)))
		return -EFAULT;

	if ((p.busnum >> 8) != dev->pci_domain ||
	    (p.busnum & 0xff) != dev->pci_bus ||
	    p.devnum != dev->pci_slot ||
	    p.funcnum != dev->pci_func)
		return -EINVAL;

	p.irq = dev->irq;

	DRM_DEBUG("%d:%d:%d => IRQ %d\n",
		  p.busnum, p.devnum, p.funcnum, p.irq);
	if (copy_to_user(argp, &p, sizeof(p)))
		return -EFAULT;
	return 0;
}

/**
 * Install IRQ handler.
 *
 * \param dev DRM device.
 * \param irq IRQ number.
 *
 * Initializes the IRQ related data, and setups drm_device::vbl_queue. Installs the handler, calling the driver
 * \c drm_driver_irq_preinstall() and \c drm_driver_irq_postinstall() functions
 * before and after the installation.
 */
static int drm_irq_install( drm_device_t *dev )
{
	int ret;
	unsigned long sh_flags=0;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
		return -EINVAL;

	if ( dev->irq == 0 )
		return -EINVAL;

	down( &dev->struct_sem );

	/* Driver must have been initialized */
	if ( !dev->dev_private ) {
		up( &dev->struct_sem );
		return -EINVAL;
	}

	if ( dev->irq_enabled ) {
		up( &dev->struct_sem );
		return -EBUSY;
	}
	dev->irq_enabled = 1;
	up( &dev->struct_sem );

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, dev->irq );

	if (drm_core_check_feature(dev, DRIVER_IRQ_VBL)) {
		init_waitqueue_head(&dev->vbl_queue);
		
		spin_lock_init( &dev->vbl_lock );
		
		INIT_LIST_HEAD( &dev->vbl_sigs.head );
		
		dev->vbl_pending = 0;
	}

				/* Before installing handler */
	dev->driver->irq_preinstall(dev);

				/* Install handler */
	if (drm_core_check_feature(dev, DRIVER_IRQ_SHARED))
		sh_flags = SA_SHIRQ;
	
	ret = request_irq( dev->irq, dev->driver->irq_handler,
			   sh_flags, dev->devname, dev );
	if ( ret < 0 ) {
		down( &dev->struct_sem );
		dev->irq_enabled = 0;
		up( &dev->struct_sem );
		return ret;
	}

				/* After installing handler */
	dev->driver->irq_postinstall(dev);

	return 0;
}

/**
 * Uninstall the IRQ handler.
 *
 * \param dev DRM device.
 *
 * Calls the driver's \c drm_driver_irq_uninstall() function, and stops the irq.
 */
int drm_irq_uninstall( drm_device_t *dev )
{
	int irq_enabled;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
		return -EINVAL;

	down( &dev->struct_sem );
	irq_enabled = dev->irq_enabled;
	dev->irq_enabled = 0;
	up( &dev->struct_sem );

	if ( !irq_enabled )
		return -EINVAL;

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, dev->irq );

	dev->driver->irq_uninstall(dev);

	free_irq( dev->irq, dev );

	return 0;
}
EXPORT_SYMBOL(drm_irq_uninstall);

/**
 * IRQ control ioctl.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument, pointing to a drm_control structure.
 * \return zero on success or a negative number on failure.
 *
 * Calls irq_install() or irq_uninstall() according to \p arg.
 */
int drm_control( struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_control_t ctl;
	
	/* if we haven't irq we fallback for compatibility reasons - this used to be a separate function in drm_dma.h */

	if ( copy_from_user( &ctl, (drm_control_t __user *)arg, sizeof(ctl) ) )
		return -EFAULT;

	switch ( ctl.func ) {
	case DRM_INST_HANDLER:
		if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
			return 0;
		if (dev->if_version < DRM_IF_VERSION(1, 2) &&
		    ctl.irq != dev->irq)
			return -EINVAL;
		return drm_irq_install( dev );
	case DRM_UNINST_HANDLER:
		if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
			return 0;
		return drm_irq_uninstall( dev );
	default:
		return -EINVAL;
	}
}

/**
 * Wait for VBLANK.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param data user argument, pointing to a drm_wait_vblank structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies the IRQ is installed. 
 *
 * If a signal is requested checks if this task has already scheduled the same signal
 * for the same vblank sequence number - nothing to be done in
 * that case. If the number of tasks waiting for the interrupt exceeds 100 the
 * function fails. Otherwise adds a new entry to drm_device::vbl_sigs for this
 * task.
 *
 * If a signal is not requested, then calls vblank_wait().
 */
int drm_wait_vblank( DRM_IOCTL_ARGS )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_wait_vblank_t __user *argp = (void __user *)data;
	drm_wait_vblank_t vblwait;
	struct timeval now;
	int ret = 0;
	unsigned int flags;

	if (!drm_core_check_feature(dev, DRIVER_IRQ_VBL))
		return -EINVAL;

	if (!dev->irq)
		return -EINVAL;

	DRM_COPY_FROM_USER_IOCTL( vblwait, argp, sizeof(vblwait) );

	switch ( vblwait.request.type & ~_DRM_VBLANK_FLAGS_MASK ) {
	case _DRM_VBLANK_RELATIVE:
		vblwait.request.sequence += atomic_read( &dev->vbl_received );
		vblwait.request.type &= ~_DRM_VBLANK_RELATIVE;
	case _DRM_VBLANK_ABSOLUTE:
		break;
	default:
		return -EINVAL;
	}

	flags = vblwait.request.type & _DRM_VBLANK_FLAGS_MASK;
	
	if ( flags & _DRM_VBLANK_SIGNAL ) {
		unsigned long irqflags;
		drm_vbl_sig_t *vbl_sig;
		
		vblwait.reply.sequence = atomic_read( &dev->vbl_received );

		spin_lock_irqsave( &dev->vbl_lock, irqflags );

		/* Check if this task has already scheduled the same signal
		 * for the same vblank sequence number; nothing to be done in
		 * that case
		 */
		list_for_each_entry( vbl_sig, &dev->vbl_sigs.head, head ) {
			if (vbl_sig->sequence == vblwait.request.sequence
			    && vbl_sig->info.si_signo == vblwait.request.signal
			    && vbl_sig->task == current)
			{
				spin_unlock_irqrestore( &dev->vbl_lock, irqflags );
				goto done;
			}
		}

		if ( dev->vbl_pending >= 100 ) {
			spin_unlock_irqrestore( &dev->vbl_lock, irqflags );
			return -EBUSY;
		}

		dev->vbl_pending++;

		spin_unlock_irqrestore( &dev->vbl_lock, irqflags );

		if ( !( vbl_sig = drm_alloc( sizeof( drm_vbl_sig_t ), DRM_MEM_DRIVER ) ) ) {
			return -ENOMEM;
		}

		memset( (void *)vbl_sig, 0, sizeof(*vbl_sig) );

		vbl_sig->sequence = vblwait.request.sequence;
		vbl_sig->info.si_signo = vblwait.request.signal;
		vbl_sig->task = current;

		spin_lock_irqsave( &dev->vbl_lock, irqflags );

		list_add_tail( (struct list_head *) vbl_sig, &dev->vbl_sigs.head );

		spin_unlock_irqrestore( &dev->vbl_lock, irqflags );
	} else {
		if (dev->driver->vblank_wait)
			ret = dev->driver->vblank_wait( dev, &vblwait.request.sequence );

		do_gettimeofday( &now );
		vblwait.reply.tval_sec = now.tv_sec;
		vblwait.reply.tval_usec = now.tv_usec;
	}

done:
	DRM_COPY_TO_USER_IOCTL( argp, vblwait, sizeof(vblwait) );

	return ret;
}

/**
 * Send the VBLANK signals.
 *
 * \param dev DRM device.
 *
 * Sends a signal for each task in drm_device::vbl_sigs and empties the list.
 *
 * If a signal is not requested, then calls vblank_wait().
 */
void drm_vbl_send_signals( drm_device_t *dev )
{
	struct list_head *list, *tmp;
	drm_vbl_sig_t *vbl_sig;
	unsigned int vbl_seq = atomic_read( &dev->vbl_received );
	unsigned long flags;

	spin_lock_irqsave( &dev->vbl_lock, flags );

	list_for_each_safe( list, tmp, &dev->vbl_sigs.head ) {
		vbl_sig = list_entry( list, drm_vbl_sig_t, head );
		if ( ( vbl_seq - vbl_sig->sequence ) <= (1<<23) ) {
			vbl_sig->info.si_code = vbl_seq;
			send_sig_info( vbl_sig->info.si_signo, &vbl_sig->info, vbl_sig->task );

			list_del( list );

			drm_free( vbl_sig, sizeof(*vbl_sig), DRM_MEM_DRIVER );

			dev->vbl_pending--;
		}
	}

	spin_unlock_irqrestore( &dev->vbl_lock, flags );
}
EXPORT_SYMBOL(drm_vbl_send_signals);


