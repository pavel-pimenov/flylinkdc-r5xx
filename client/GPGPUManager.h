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

#ifndef DCPLUSPLUS_DCPP_GPGPUMANAGER_H
#define DCPLUSPLUS_DCPP_GPGPUMANAGER_H

#ifdef FLYLINKDC_USE_GPU_TTH
#include <CL/opencl.h>


#include <tuple>
#include "Singleton.h"

class GPGPUTTHManager;

class GPGPUPlatform
{
	public:
		typedef uint32_t Status;
		
		static const Status GPGPU_OK = 0;
		static const Status GPGPU_E_KRN_EXCECUTE = 1;
		static const Status GPGPU_E_PLATFORM_INIT = 2;
		static const Status GPGPU_E_DEVICE_INIT = 4;
		static const Status GPGPU_E_MEMORY = 8;
		static const Status GPGPU_E_DEVICE_INFO = 16;
		
		GPGPUPlatform() : i_cur_dev(0) { }
		
		virtual void select_device(int num) = 0;
		virtual string get_dev_name(int num) = 0;
		virtual int get_dev_cnt() = 0;
		virtual string get_platform_name() = 0;
		virtual bool finish() = 0;
		virtual Status get_status() = 0;
		
		/* Functions that will use GPU */
		virtual bool krn_ttr(uint8_t *data, uint64_t bc, uint64_t last_bs, uint64_t res[3])
		{
			return false;
		}
		
	protected:
		int i_cur_dev;
};

class GPGPUManager
{
	public:
		GPGPUManager();
		~GPGPUManager();
		
		void select(int pfm_idx)
		{
			i_cur_pfm = pfm_idx;
		}
		GPGPUPlatform * get()
		{
			return platforms[i_cur_pfm];
		}
		
	private:
		vector<GPGPUPlatform *> platforms;
		int i_cur_pfm;
};

class GPGPUTTHManager : public GPGPUManager, public Singleton<GPGPUTTHManager>
{
	public:
		GPGPUTTHManager() { }
		~GPGPUTTHManager() { }
};

class GPGPUOpenCL : public GPGPUPlatform
{
	public:
		typedef std::tuple<cl_platform_id, cl_device_id, string> OCLGPGPUDevice;
		
		GPGPUOpenCL();
		~GPGPUOpenCL();
		
		void select_device(int num);
		string get_dev_name(int num)
		{
			return std::get<2>(devs[num]);
		}
		int get_dev_cnt()
		{
			return devs.size();
		}
		string get_platform_name()
		{
			return pfm_name;
		}
		bool finish();
		Status get_status()
		{
			return sts;
		}
		
		bool krn_ttr(uint8_t *data, uint64_t bc, uint64_t last_bs, uint64_t res[3]);
		
	private:
		string pfm_name; // Name of GPGPU platform (OpenCL, CUDA, DirectCompute etc.)
		// Don't confuse with OpenCL platforms which get by
		// calling clGetPlatformIDs
		vector<OCLGPGPUDevice> devs;
		int cur_dev_num;
		
		cl_context cur_cntxt;
		cl_command_queue cur_cmd_q;
		
		cl_program cur_prog;
		cl_kernel cur_ttr_krn;
		
		Status sts;
		
		/* Number of work items with witch excecute kernel (1 dim and 1 workgroup) */
		size_t wis1d1wg;
		
		/* Gets max work items for one workgroup one dimentional kernel excecuting */
		size_t get_1d_max_wg_wi(int dev_num);
};

#endif // FLYLINKDC_USE_GPU_TTH
#endif // !defined(GPGPUMANAGER_H)