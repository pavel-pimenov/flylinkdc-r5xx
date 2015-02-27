/*
* Copyright (C) 2015 ecl1pse
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "stdinc.h"

#ifdef FLYLINKDC_USE_GPU_TTH

#include "GPGPUManager.h"
#include "SettingsManager.h"

GPGPUOpenCL::GPGPUOpenCL()
	: pfm_name("OpenCL"),
	  cur_dev_num(-1),
	  cur_ttr_krn(0), cur_prog(0),
	  cur_cmd_q(0), cur_cntxt(0),
	  sts(GPGPU_S_DEVICE_NOT_SELECTED)
{
	cl_uint npfms;
	cl_platform_id *pfm_ids;
	size_t pfm_name_sz;
	char *pfm_name;
	
	cl_uint ndevs;
	cl_device_id *dev_ids;
	size_t dev_name_sz;
	char *dev_name;
	
	unsigned i, j;
	
	cl_int i_ret;
	
	try
	{
		i_ret = clGetPlatformIDs(0, 0, &npfms);
		pfm_ids = new cl_platform_id[npfms];
		i_ret |= clGetPlatformIDs(npfms, pfm_ids, 0);
		
		if (i_ret != CL_SUCCESS)
		{
			sts |= GPGPU_E_PLATFORM_INIT;
			return;
		}
		
		for (i = 0; i < npfms; ++i)
		{
			i_ret = clGetPlatformInfo(pfm_ids[i], CL_PLATFORM_NAME, 0, 0, &pfm_name_sz);
			pfm_name = new char[pfm_name_sz];
			i_ret |= clGetPlatformInfo(pfm_ids[i], CL_PLATFORM_NAME, pfm_name_sz, pfm_name, 0);
			
			i_ret |= clGetDeviceIDs(pfm_ids[i], CL_DEVICE_TYPE_GPU, 0, 0, &ndevs);
			dev_ids = new cl_device_id[ndevs];
			i_ret |= clGetDeviceIDs(pfm_ids[i], CL_DEVICE_TYPE_GPU, ndevs, dev_ids, 0);
			
			if (i_ret != CL_SUCCESS) continue;
			
			for (j = 0; j < ndevs; ++j)
			{
				i_ret = clGetDeviceInfo(dev_ids[j], CL_DEVICE_NAME, 0, 0, &dev_name_sz);
				dev_name = new char[dev_name_sz];
				i_ret |= clGetDeviceInfo(dev_ids[j], CL_DEVICE_NAME, dev_name_sz, dev_name, 0);
				
				if (i_ret != CL_SUCCESS) continue;
				
				devs.push_back( OCLGPGPUDevice(pfm_ids[i], dev_ids[j], string(pfm_name)+string(" - ")+string(dev_name)) );
				
				delete[] dev_name;
			}
			
			delete[] dev_ids;
			delete[] pfm_name;
		}
		
		delete[] pfm_ids;
	}
	catch (std::bad_alloc)
	{
		sts |= GPGPU_E_PLATFORM_INIT;
	}
}

GPGPUOpenCL::~GPGPUOpenCL()
{
	clReleaseKernel(cur_ttr_krn);
	clReleaseProgram(cur_prog);
	clReleaseCommandQueue(cur_cmd_q);
	clReleaseContext(cur_cntxt);
}

void GPGPUOpenCL::select_device(int num)
{
	cl_context_properties ctx_props[3];

	FILE *f = 0;
	long sz;
	char *src;
	string ph_to_krn(Util::getGPGPUPath() + "src" PATH_SEPARATOR_STR "ttr.cl");

	cl_int i_res;

	dcassert( num >= 0 && num < (int)devs.size() );

	clReleaseKernel(cur_ttr_krn);
	clReleaseProgram(cur_prog);
	clReleaseCommandQueue(cur_cmd_q);
	clReleaseContext(cur_cntxt);

	ctx_props[0] = CL_CONTEXT_PLATFORM;
	ctx_props[1] = (cl_context_properties) devs[num].m_pfm_id;
	ctx_props[2] = 0;
	cur_cntxt = clCreateContext(ctx_props, 1, &devs[num].m_dev_id, 0, 0, 0);
	
	if (!cur_cntxt) goto dev_init_error;
	
	cur_cmd_q = clCreateCommandQueue(cur_cntxt, devs[num].m_dev_id, 0, 0);
	
	if (!cur_cmd_q) goto dev_init_error;
	
	/* Loading and building program for GPU */
	f = fopen(ph_to_krn.c_str(), "rb");
	
	if (!f) goto dev_init_error;
	
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	
	src = (char *)malloc(sz);
	if (!src) goto dev_init_error;
	
	fread(src, 1, sz, f);
	
	cur_prog = clCreateProgramWithSource(cur_cntxt, 1,
		(const char **)&src, (const size_t *)&sz, 0);

	if (!cur_prog) goto dev_init_error;
	
	clBuildProgram(cur_prog, 0, 0, "", 0, 0);
	
	free(src);
	fclose(f);
	
	cur_ttr_krn = clCreateKernel(cur_prog, "ttr", 0);
	
	if (!cur_ttr_krn) goto dev_init_error;
	
	cur_dev_num = num;
	sts = GPGPU_OK;
	
	wis1d1wg = get_1d_max_wg_wi(cur_dev_num);
	
	return;
	
dev_init_error:
	sts |= GPGPU_E_DEVICE_INIT | GPGPU_S_DEVICE_NOT_SELECTED;
	
	if (f) fclose(f);
	
	clReleaseKernel(cur_ttr_krn);
	clReleaseProgram(cur_prog);
	clReleaseCommandQueue(cur_cmd_q);
	clReleaseContext(cur_cntxt);
	
	cur_ttr_krn = 0;
	cur_prog = 0;
	cur_cmd_q = 0;
	cur_cntxt = 0;
}

bool GPGPUOpenCL::krn_ttr(uint8_t *data, uint64_t bc, uint64_t last_bs, uint64_t res[3])
{
	const uint64_t TTBLOCK_SIZE = 1024ULL;
	const uint64_t TH_BYTES = 24ULL;
	
	cl_mem mem_data, mem_res;
	
	cl_int i_res;
	
	bool b_ret = true;
	
	dcassert(bc);
	
	mem_data = clCreateBuffer(cur_cntxt,
		CL_MEM_READ_WRITE,
		bc * TTBLOCK_SIZE, 0, 0);

	mem_res = clCreateBuffer(cur_cntxt,
		CL_MEM_WRITE_ONLY,
		TH_BYTES, 0, 0);

	if (!mem_data || !mem_res)
	{
		sts |= GPGPU_E_MEMORY;
		b_ret = false;
		goto exit;
	}

	i_res = clEnqueueWriteBuffer(cur_cmd_q, mem_data,
		CL_FALSE, 0,
		(bc - 1)*TTBLOCK_SIZE + last_bs, data,
		0, 0, 0);

	if (i_res != CL_SUCCESS)
	{
		sts |= GPGPU_E_MEMORY;
		b_ret = false;
		goto exit;
	}
	
	i_res = clSetKernelArg(cur_ttr_krn, 0, sizeof(cl_mem), &mem_data);
	i_res |= clSetKernelArg(cur_ttr_krn, 1, sizeof(cl_ulong), &bc);
	i_res |= clSetKernelArg(cur_ttr_krn, 2, sizeof(cl_ulong), &last_bs);
	i_res |= clSetKernelArg(cur_ttr_krn, 3, sizeof(cl_mem), &mem_res);

	//clEnqueueTask(cur_cmd_q, cur_ttr_krn, 0, 0, 0);
	i_res |= clEnqueueNDRangeKernel(cur_cmd_q, cur_ttr_krn, 1, 0, &wis1d1wg, &wis1d1wg, 0, 0, 0);
	
	if (i_res != CL_SUCCESS)
	{
		sts |= GPGPU_E_KRN_EXCECUTE;
		b_ret = false;
		goto exit;
	}
	
	i_res = clEnqueueReadBuffer(cur_cmd_q, mem_res,
		CL_FALSE, 0,
		TH_BYTES, res,
		0, 0, 0);

	if (i_res != CL_SUCCESS)
	{
		sts |= GPGPU_E_MEMORY;
		b_ret = false;
		goto exit;
	}
	
exit:
	clReleaseMemObject(mem_res);
	clReleaseMemObject(mem_data);
	
	return b_ret;
}

bool GPGPUOpenCL::finish()
{
	if (clFinish(cur_cmd_q) != CL_SUCCESS)
	{
		return false;
	}
	
	return true;
}

size_t GPGPUOpenCL::get_1d_max_wg_wi(int dev_num)
{
	size_t max_wg_sz;
	cl_uint max_wi_dims;
	
	size_t *max_wi_szs;
	size_t sz;
	
	cl_int i_ret;
	
	cl_device_id dev_id;
	
	size_t res = 0;
	
    dcassert( dev_num >= 0 && dev_num < (int)devs.size() );
	
	dev_id = devs[dev_num].m_dev_id;
	
	i_ret = clGetDeviceInfo(dev_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &max_wg_sz, 0);
	i_ret |= clGetDeviceInfo(dev_id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &max_wi_dims, 0);
	
	if (i_ret != CL_SUCCESS)
	{
		sts |= GPGPU_E_DEVICE_INFO;
		return 0;
	}
	
	if (!max_wi_dims) return 0;
	
	i_ret = clGetDeviceInfo(dev_id, CL_DEVICE_MAX_WORK_ITEM_SIZES, 0, 0, &sz);
	
	dcassert(sizeof(size_t)*max_wi_dims == sz);
	
	try
	{
		max_wi_szs = new size_t[max_wi_dims];
		
		i_ret |= clGetDeviceInfo(dev_id, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t) * max_wi_dims, max_wi_szs, 0);
		
		res = min(max_wg_sz, max_wi_szs[0]);
		
		delete[] max_wi_szs;
	}
	catch (std::bad_alloc)
	{
		sts |= GPGPU_E_DEVICE_INFO;
		res = 0;
	}
	
	if (i_ret != CL_SUCCESS)
	{
		sts |= GPGPU_E_DEVICE_INFO;
		return 0;
	}
	
	return res;
}

GPGPUManager::GPGPUManager()
	: i_cur_pfm(-1)
{
	platforms.push_back( new GPGPUOpenCL );
	i_cur_pfm = 0;
}

GPGPUManager::~GPGPUManager()
{
	for ( auto i = platforms.begin(); i != platforms.end(); ++i )
	{
		delete *i;
	}
}

GPGPUTTHManager::GPGPUTTHManager()
{
	const int dcnt = get()->get_dev_cnt();
	const int set_dnum = SETTING(TTH_GPU_DEV_NUM);
	const string& s_set_dname = SETTING(GPU_DEV_NAME_FOR_TTH_COMP);

	if (set_dnum >= 0 && set_dnum < dcnt)
	{
		const string& s_dname = get()->get_dev_name(set_dnum);

		if (s_dname == s_set_dname)
		{
			get()->select_device(set_dnum);
		}
	}
}

#endif // FLYLINKDC_USE_GPU_TTH