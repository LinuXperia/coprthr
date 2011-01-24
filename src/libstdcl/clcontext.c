/* clcontext.c
 *
 * Copyright (c) 2009 Brown Deer Technology, LLC.  All Rights Reserved.
 *
 * This software was developed by Brown Deer Technology, LLC.
 * For more information contact info@browndeertechnology.com
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3 (LGPLv3)
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* DAR */


/* XXX to do, add err code checks, other safety checks * -DAR */
/* XXX to do, clvplat_destroy should automatically release all txts -DAR */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/queue.h>
#include <pthread.h>

#include <CL/cl.h>

#include "util.h"
#include "clinit.h"
#include "clfcn.h"
#include "clcontext.h"

#ifdef DEFAULT_OPENCL_PLATFORM
#define DEFAULT_PLATFORM_NAME DEFAULT_OPENCL_PLATFORM
#else
#define DEFAULT_PLATFORM_NAME "ATI"
#endif


static cl_platform_id
__get_platformid(
 cl_uint nplatform, cl_platform_id* platforms, const char* platform_name
);


/* XXX note that presently ndevmax is ignored -DAR */

CONTEXT* 
clcontext_create( 
	const char* platform_name, 
	int devtyp, size_t ndevmax, 
	cl_context_properties* ctxprop_ext, 
	int flag
//	cl_platform_id platformid, int devtyp, size_t ndevmax, int flag
)
{

	int n;

	DEBUG(__FILE__,__LINE__,"clcontext_create() called");

	/* XXX ndev>0 should allow limit placed on number of devices in vp -DAR */
	if (ndevmax) 
		WARN(__FILE__,__LINE__,"__clcontext_create(): ndevmax argument ignored");

	int err = 0;
	int i;
	size_t devlist_sz;
	CONTEXT* cp = 0;


	/***
	 *** allocate CONTEXT struct
	 ***/

	DEBUG(__FILE__,__LINE__,
		"clcontext_create: sizeof CONTEXT %d",sizeof(CONTEXT));

//	cp = (CONTEXT*)malloc(sizeof(CONTEXT));
	assert(sizeof(CONTEXT)<getpagesize());
	if (posix_memalign((void**)&cp,getpagesize(),sizeof(CONTEXT))) {
		WARN(__FILE__,__LINE__,"posix_memalign failed");
	}

	DEBUG(__FILE__,__LINE__,"clcontext_create: context_ptr=%p",cp);
	
	if ((intptr_t)cp & (getpagesize()-1)) {
		ERROR(__FILE__,__LINE__,
			"clcontext_create: fatal error: unaligned context_ptr");
		exit(-1);
	}

	if (!cp) { errno=ENOMEM; return(0); }


   /***
    *** get platform id
    ***/

   cl_platform_id* platforms = 0;
   cl_uint nplatforms;

   char info[1024];

   clGetPlatformIDs(0,0,&nplatforms);

	printf("XXX %d\n",nplatforms);

   if (nplatforms) {

      platforms = (cl_platform_id*)malloc(nplatforms*sizeof(cl_platform_id));
      clGetPlatformIDs(nplatforms,platforms,0);

      for(i=0;i<nplatforms;i++) { 

         char info[1024];

         DEBUG(__FILE__,__LINE__,"_libstdcl_init: available platform:");

         clGetPlatformInfo(platforms[i],CL_PLATFORM_PROFILE,1024,info,0);
         DEBUG(__FILE__,__LINE__,
            "_libstdcl_init: [%p]CL_PLATFORM_PROFILE=%s",platforms[i],info);

         clGetPlatformInfo(platforms[i],CL_PLATFORM_VERSION,1024,info,0);
         DEBUG(__FILE__,__LINE__,
            "_libstdcl_init: [%p]CL_PLATFORM_VERSION=%s",platforms[i],info);

         clGetPlatformInfo(platforms[i],CL_PLATFORM_NAME,1024,info,0);
         DEBUG(__FILE__,__LINE__,
            "_libstdcl_init: [%p]CL_PLATFORM_NAME=%s",platforms[i],info);

         clGetPlatformInfo(platforms[i],CL_PLATFORM_VENDOR,1024,info,0);
         DEBUG(__FILE__,__LINE__,
            "_libstdcl_init: [%p]CL_PLATFORM_VENDOR=%s",platforms[i],info);

         clGetPlatformInfo(platforms[i],CL_PLATFORM_EXTENSIONS,1024,info,0);
         DEBUG(__FILE__,__LINE__,
            "_libstdcl_init: [%p]CL_PLATFORM_EXTENSIONS=%s",platforms[i],info);

      }

   } else {

      WARN(__FILE__,__LINE__,
         "_libstdcl_init: no platforms found, continue and hope for the best");

   }

	cl_platform_id platformid 
		= __get_platformid(nplatforms, platforms, platform_name);

	DEBUG(__FILE__,__LINE__,"clcontext_create: platformid=%p",platformid);



	/***
	 *** create context
	 ***/

	int nctxprop = 0;

	while (ctxprop_ext != 0 && ctxprop_ext[nctxprop] != 0) ++nctxprop;

//	cl_context_properties ctxprop[3] = {
//		(cl_context_properties)CL_CONTEXT_PLATFORM,
//		(cl_context_properties)platformid,
//		(cl_context_properties)0
//	};

	nctxprop += 3;

	cl_context_properties* ctxprop 
		= (cl_context_properties*)malloc(nctxprop*sizeof(cl_context_properties));

	ctxprop[0] = (cl_context_properties)CL_CONTEXT_PLATFORM;
	ctxprop[1] = (cl_context_properties)platformid;

	for(i=0;i<nctxprop-3;i++) ctxprop[2+i] = ctxprop_ext[i];

	ctxprop[nctxprop-1] =  (cl_context_properties)0;

	cp->ctx = clCreateContextFromType(ctxprop,devtyp,0,0,&err);

	if (!cp->ctx) {
		free(cp);
		return((CONTEXT*)0);
	}

	err = clGetContextInfo(cp->ctx,CL_CONTEXT_DEVICES,0,0,&devlist_sz);
	cp->ndev = devlist_sz/sizeof(cl_device_id);
	cp->dev = (cl_device_id*)malloc(devlist_sz);
	err = clGetContextInfo(cp->ctx,CL_CONTEXT_DEVICES,devlist_sz,cp->dev,0);

	DEBUG(__FILE__,__LINE__,"number of devices %d",cp->ndev);


	/***
	 *** create command queues
	 ***/

	cp->cmdq = (cl_command_queue*)malloc(sizeof(cl_command_queue)*cp->ndev);

	DEBUG(__FILE__,__LINE__,"will try to create cmdq");

// XXX something is broken in clCreateCommandQueue, using lazy creation
// XXX as a workaround -DAR
//	{
		cl_command_queue_properties prop = 00;
		prop |= CL_QUEUE_PROFILING_ENABLE; /* XXX this should be choice -DAR */
		for(i=0;i<cp->ndev;i++) {
			cp->cmdq[i] = clCreateCommandQueue(cp->ctx,cp->dev[i],prop,&err);

			DEBUG(__FILE__,__LINE__,"clcontext_create: error from create cmdq %d (%p)\n",
				err,cp->cmdq[i]);
		}
//	}
//	printf("WARNING CMDQs NOT CREATED\n");
//	for(i=0;i<cp->ndev;i++) cp->cmdq[i] = (cl_command_queue)0;



	/***
	 *** init context resources
	 ***/

	LIST_INIT(&cp->prgs_listhead);

	LIST_INIT(&cp->txt_listhead);

	LIST_INIT(&cp->memd_listhead);


//	struct _prgs_struct* prgs 
//		= (struct _prgs_struct*)malloc(sizeof(struct _prgs_struct));
//	prgs->len=-1;
//	LIST_INSERT_HEAD(&cp->prgs_listhead, prgs, prgs_list);
//
//	prgs = (struct _prgs_struct*)malloc(sizeof(struct _prgs_struct));
//	prgs->len=-2;
//	LIST_INSERT_HEAD(&cp->prgs_listhead, prgs, prgs_list);

/*
	printf("%p searching _proc_cl for prgs...\n",_proc_cl.clstrtab);
	printf("%s\n",&_proc_cl.clstrtab[1]);
	struct clprgs_entry* sp;
	for(n=0,sp=_proc_cl.clprgs;n<_proc_cl.clprgs_n;n++,sp++) {
		printf("found %s (%d bytes)\n",&_proc_cl.clstrtab[sp->e_name],sp->e_size);
		struct _prgs_struct* prgs = (struct _prgs_struct*)
			clload(cp,_proc_cl.cltexts+sp->e_offset,sp->e_size,0);
	}
*/


	/*** 
	 *** initialize event lists
	 ***/
	
//	cp->nkev = cp->kev_first = cp->kev_free = 0;
	cp->kev = (struct _event_list_struct*)
		malloc(cp->ndev*sizeof(struct _event_list_struct));

	for(i=0;i<cp->ndev;i++) {
		cp->kev[i].nev = cp->kev[i].ev_first = cp->kev[i].ev_free = 0;
	}

//	cp->nmev = cp->mev_first = cp->mev_free = 0;
	cp->mev = (struct _event_list_struct*)
		malloc(cp->ndev*sizeof(struct _event_list_struct));

	for(i=0;i<cp->ndev;i++) {
		cp->mev[i].nev = cp->mev[i].ev_first = cp->mev[i].ev_free = 0;
	}

#ifdef ENABLE_CLEXPORT
	cp->ndev_v = 0;
	cp->extd = 0;
	cp->imtd = 0;
#endif


	if (platforms) free(platforms);

	return(cp);

}



int
clcontext_destroy(CONTEXT* cp)
{
	DEBUG(__FILE__,__LINE__,"clcontext_destroy() called");

	int i;
	int err = 0;

	if (!cp) return(0);

	size_t ndev = cp->ndev;

	DEBUG(__FILE__,__LINE__,"clcontext_destroy: ndev %d",ndev);

	if (cp->kev) free(cp->kev);

//	struct _prgs_struct* prgs;
//   for (
//		prgs = cp->prgs_listhead.lh_first; prgs != 0; 
//		prgs = prgs->prgs_list.le_next
//	) {
//      printf("%d\n",prgs->len);
//   }

	while (cp->prgs_listhead.lh_first != 0)   
//		LIST_REMOVE(cp->prgs_listhead.lh_first, prgs_list);
		clclose(cp,cp->prgs_listhead.lh_first);

	/* XXX here force detach of any clmalloc() memory -DAR */

	for(i=0;i<ndev;i++) {
		DEBUG(__FILE__,__LINE__,"checking cmdq for release %p",cp->cmdq[i]);
		if (cp->cmdq[i]) err |= clReleaseCommandQueue(cp->cmdq[i]);
	}

	DEBUG(__FILE__,__LINE__,"clcontext_destroy: released cmdq's");

	err |= clReleaseContext(cp->ctx);

	DEBUG(__FILE__,__LINE__,"clcontext_destroy: released ctx");

	if (cp->cmdq) free(cp->cmdq);
	DEBUG(__FILE__,__LINE__,"clcontext_destroy: free'd cmdq\n");
	if (cp->dev) free(cp->dev);
	DEBUG(__FILE__,__LINE__,"clcontext_destroy: free'd dev\n");
	free(cp);

	return(0);
}



/* XXX there is no check of err code, this should be added -DAR */

int clgetdevinfo( CONTEXT* cp, struct cldev_info* info)
{

	if (!cp) { errno=ENOENT; return(-1); }

	int err;
	size_t ndev = cp->ndev;
	int i,n;
	cl_device_id* d;
	struct cldev_info* di;

	if (cp->dev) {

		for(n=0,d=cp->dev,di=info;n<ndev;n++,d++,di++) {

//			printf("%p\n",di);

			err = clGetDeviceInfo(
  		    	*d,CL_DEVICE_TYPE,
  		    	sizeof(cl_device_type),&di->dev_type,0
  		 	);

		   err = clGetDeviceInfo(
  	   		*d,CL_DEVICE_VENDOR_ID,
  		    	sizeof(cl_uint),&di->dev_vendor_id,0
  		 	);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(cl_uint),&di->dev_max_core,0
			);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
				sizeof(cl_uint),&di->dev_max_wi_dim,0
			);

			assert(di->dev_max_wi_dim<=4);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_WORK_ITEM_SIZES,
				di->dev_max_wi_dim*sizeof(size_t),di->dev_max_wi_sz,0
			);
			di->dev_max_wi_sz_is_symmetric=1;
     		for(i=0;i<di->dev_max_wi_dim;i++)
     	   	if(di->dev_max_wi_sz[0]!=di->dev_max_wi_sz[i]) 
					di->dev_max_wi_sz_is_symmetric=0;

			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_WORK_GROUP_SIZE,
				sizeof(size_t),&di->dev_max_wg_sz,0
			);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
				sizeof(cl_uint),&di->dev_pref_vec_char,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
				sizeof(cl_uint),&di->dev_pref_vec_short,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
				sizeof(cl_uint),&di->dev_pref_vec_int,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
				sizeof(cl_uint),&di->dev_pref_vec_long,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,
				sizeof(cl_uint),&di->dev_pref_vec_float,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,
				sizeof(cl_uint),&di->dev_pref_vec_double,0
			);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_CLOCK_FREQUENCY,
				sizeof(cl_uint),&di->dev_max_freq,0
			);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_ADDRESS_BITS,
				sizeof(cl_bitfield),&di->dev_addr_bits,0
			);


			cl_ulong ultmp;

			err = clGetDeviceInfo(
            *d,CL_DEVICE_MAX_MEM_ALLOC_SIZE,
            sizeof(cl_ulong),&ultmp,0
			);	
//			printf("XXX CL_DEVICE_GLOBAL_MEM_SIZE %d\n",(int)ultmp);


			/*
			 * check image support
			 */

			err = clGetDeviceInfo(
				*d,CL_DEVICE_IMAGE_SUPPORT,
				sizeof(cl_bool),&di->dev_img_sup,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_READ_IMAGE_ARGS,
				sizeof(cl_uint),&di->dev_max_img_args_r,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_WRITE_IMAGE_ARGS,
				sizeof(cl_uint),&di->dev_max_img_args_w,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_IMAGE2D_MAX_WIDTH,
				sizeof(size_t),&di->dev_img2d_max_width,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_IMAGE2D_MAX_HEIGHT,
				sizeof(size_t),&di->dev_img2d_max_height,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_IMAGE2D_MAX_WIDTH,
				sizeof(size_t),&di->dev_img3d_max_width,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_IMAGE2D_MAX_HEIGHT,
				sizeof(size_t),&di->dev_img3d_max_height,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_IMAGE3D_MAX_DEPTH,
				sizeof(size_t),&di->dev_img3d_max_height,0
			);
			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_SAMPLERS,
				sizeof(cl_uint),&di->dev_max_samplers,0
			);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_MAX_PARAMETER_SIZE,
				sizeof(size_t),&di->dev_max_param_sz,0
			);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_MEM_BASE_ADDR_ALIGN,
				sizeof(cl_uint),&di->dev_mem_base_addr_align,0
			);

			err = clGetDeviceInfo(*d,CL_DEVICE_NAME,256,di->dev_name,0);
			err = clGetDeviceInfo(*d,CL_DEVICE_VENDOR,256,di->dev_vendor,0);
			err = clGetDeviceInfo(*d,CL_DEVICE_VERSION,256,di->dev_version,0);
			err = clGetDeviceInfo(*d,CL_DRIVER_VERSION,256,di->dev_drv_version,0);

			err = clGetDeviceInfo(
				*d,CL_DEVICE_LOCAL_MEM_SIZE,
			 	sizeof(cl_ulong),&di->dev_local_mem_sz,0
			);
	
		}

	}

	return(0);
}



void clfreport_devinfo( FILE* fp, size_t ndev, struct cldev_info* info )
{
	int i,n;
	struct cldev_info* di;

	if (info) for(n=0,di=info;n<ndev;n++,di++) {	

   	fprintf(fp,"device %d: ",n);

   	fprintf(fp,"CL_DEVICE_TYPE=");
   	if (di->dev_type&CL_DEVICE_TYPE_CPU) fprintf(fp," CPU");
   	if (di->dev_type&CL_DEVICE_TYPE_GPU) fprintf(fp," GPU");
   	if (di->dev_type&CL_DEVICE_TYPE_ACCELERATOR) fprintf(fp," ACCELERATOR");
   	if (di->dev_type&CL_DEVICE_TYPE_DEFAULT) fprintf(fp," DEFAULT");
   	fprintf(fp,"\n");

   	fprintf(fp,"CL_DEVICE_VENDOR_ID=%d\n",di->dev_vendor_id);
		fprintf(fp,"CL_DEVICE_MAX_COMPUTE_UNITS=%d\n",di->dev_max_core);
		fprintf(fp,"CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS=%d\n",di->dev_max_wi_dim);

   	if (di->dev_max_wi_dim>0) {

      	fprintf(fp,"CL_DEVICE_MAX_WORK_ITEM_SIZES=");
			
      	if (di->dev_max_wi_sz_is_symmetric)
         	fprintf(fp," %d (symmetric)\n",di->dev_max_wi_sz[0]);
      	else
         	for(i=0;i<di->dev_max_wi_dim;i++) 
					fprintf(fp," [%d]%d\n",i,di->dev_max_wi_sz[i]);
   	}

   	fprintf(fp,"CL_DEVICE_MAX_WORK_GROUP_SIZE=%d\n",di->dev_max_wg_sz);
   	fprintf(
			fp,"CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR=%d\n",di->dev_pref_vec_char
		);
   	fprintf(
			fp,"CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT=%d\n",
			di->dev_pref_vec_short
		);
   	fprintf(
			fp,"CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT=%d\n",di->dev_pref_vec_int
		);
   	fprintf(
			fp,"CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG=%d\n",di->dev_pref_vec_long
		);
   	fprintf(
			fp,"CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT=%d\n",di->dev_pref_vec_float
		);
   	fprintf(
			fp,"CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE=%d\n",
			di->dev_pref_vec_double
		);  
   	fprintf(fp,"CL_DEVICE_MAX_CLOCK_FREQUENCY=%d\n",di->dev_max_freq);

// XXX SDK does not define CL_DEVICE_ADDRESS_32_BITS,CL_DEVICE_ADDRESS_64_BITS
// 	fprintf(fp,"CL_DEVICE_ADDRESS_BITS=");
// 	if (di->dev_addr_bits&CL_DEVICE_ADDRESS_32_BIT) fprintf(fp," 32-bit");
// 	if (di->dev_addr_bits&CL_DEVICE_ADDRESS_64_BITS) fprintf(fp," 64-bit");
// 	if (!di->dev_addr_bits) fprintf(fp," no");
// 	fprintf(fp," address space support\n");
		fprintf(fp,"CL_DEVICE_ADDRESS_BITS=0x%x\n",di->dev_addr_bits);

	   if (di->dev_img_sup) {
      	fprintf(fp,"CL_DEVICE_IMAGE_SUPPORT=true\n");
      	fprintf(
				fp,"CL_DEVICE_MAX_READ_IMAGE_ARGS=%d\n",di->dev_max_img_args_r
			);
      	fprintf(
				fp,"CL_DEVICE_MAX_WRITE_IMAGE_ARGS=%d\n",di->dev_max_img_args_w
			);
      	fprintf(
				fp,"CL_DEVICE_IMAGE2D_MAX_WIDTH=%d\n",di->dev_img2d_max_width
			);
      	fprintf(
				fp,"CL_DEVICE_IMAGE2D_MAX_HEIGHT=%d\n",di->dev_img2d_max_height
			);
      	fprintf(
				fp,"CL_DEVICE_IMAGE3D_MAX_WIDTH=%d\n",di->dev_img3d_max_width
			);
      	fprintf(
				fp,"CL_DEVICE_IMAGE3D_MAX_HEIGHT=%d\n",di->dev_img3d_max_height
			);
      	fprintf(
				fp,"CL_DEVICE_IMAGE3D_MAX_DEPTH=%d\n",di->dev_img3d_max_depth
			);
   	} else {
      	fprintf(fp,"CL_DEVICE_IMAGE_SUPPORT=false\n");
   	}

   	fprintf(fp,"CL_DEVICE_MAX_PARAMETER_SIZE=%d\n",di->dev_max_param_sz);
   	fprintf(
			fp,"CL_DEVICE_MEM_BASE_ADDRESS_ALIGN=%d\n",di->dev_mem_base_addr_align
		);

   	fprintf(fp,"CL_DEVICE_NAME=%s\n",di->dev_name);
   	fprintf(fp,"CL_DEVICE_VENDOR=%s\n",di->dev_vendor);
   	fprintf(fp,"CL_DEVICE_VERSION=%s\n",di->dev_version);
   	fprintf(fp,"CL_DRIVER_VERSION=%s\n",di->dev_drv_version);

   	fprintf(fp,"CL_DEVICE_LOCAL_MEM_SIZE=%d\n",di->dev_local_mem_sz);

	}

}

int clstat( CONTEXT* cp, struct clstat_info* st)
{
	if (!cp || !st) return(-1);

	st->impid = cp->impid;
	st->ndev = cp->ndev;
	st->nprg = cp->nprg;
	st->nkrn = cp->nkrn;

	return(0);
}

cl_uint clgetndev( CONTEXT* cp )
{
	if (!cp) return (0);
	return (cp->ndev);
}

static cl_platform_id
__get_platformid(
 cl_uint nplatform, cl_platform_id* platforms, const char* platform_name
)  
{

   DEBUG(__FILE__,__LINE__, 
		"__get_platformid: platform_name |%s|",platform_name);

   int i,j;
   char name[256];
//   _getenv_token(env_var,"platform_name",name,256);

//   if (name[0] == '\0') {
   if (platform_name == 0 || platform_name[0] == '\0') {

      strcpy(name,DEFAULT_PLATFORM_NAME);

      DEBUG(__FILE__,__LINE__,
         "__get_platformid: use default platform_name |%s|",name);

   } else {

      strncpy(name,platform_name,256);

//      DEBUG(__FILE__,__LINE__,
//         "__get_platformid: environment platform_name |%s|",name);

   }

   if (platforms) for(i=0;i<nplatform;i++) {

      char info[1024];
      clGetPlatformInfo(platforms[i],CL_PLATFORM_NAME,1024,info,0);

      DEBUG(__FILE__,__LINE__,"__get_platformid: compare |%s|%s|",name,info);

      if (!strncasecmp(name,info,strlen(name))) {

         DEBUG(__FILE__,__LINE__,"__get_platformid: "
            " match %s %s",name,info);

         return(platforms[i]);

      }

   }

   if (nplatform > 0) return(platforms[0]); /* default to first one */

   return((cl_platform_id)(-1)); /* none found */

}
