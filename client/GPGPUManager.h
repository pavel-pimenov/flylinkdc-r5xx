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
#include <vector>

#include "Singleton.h"
#include "CFlyThread.h"

class GPGPUTTHManager;

class GPGPUPlatform
{
	public:
		typedef uint32_t Status;
		
		static const Status GPGPU_OK = 0;
		static const Status GPGPU_S_DEVICE_NOT_SELECTED = 1UL;
		
		static const Status GPGPU_E_KRN_EXCECUTE = (1UL << 31);
		static const Status GPGPU_E_PLATFORM_INIT = (1UL << 30);
		static const Status GPGPU_E_DEVICE_INIT = (1UL << 29);
		static const Status GPGPU_E_MEMORY = (1UL << 28);
		static const Status GPGPU_E_DEVICE_INFO = (1UL << 27);
		
		GPGPUPlatform() { }
		
		virtual void select_device(int num) = 0;
		virtual int get_cur_dev() = 0;
		virtual const string& get_dev_name(int num) = 0;
		virtual int get_dev_cnt() = 0;
		virtual const string& get_platform_name() = 0;
		virtual Status get_status() = 0;
		
		/* Functions that will use GPU */
		virtual bool krn_ttr(uint8_t *data, uint64_t bc, uint64_t last_bs, uint64_t res[3])
		{
			return false;
		}
};

class GPGPUManager
{
	public:
		GPGPUManager();
		~GPGPUManager();
		
		void select(int pfm_idx)
		{
			CFlyFastLock(cs);
			
			if (pfm_idx >= 0 && pfm_idx < (int)platforms.size())
			{
				i_cur_pfm = pfm_idx;
			}
			else
			{
				// throw Invalid Platform Index exception
			}
		}
		
		GPGPUPlatform * get()
		{
			CFlyFastLock(cs);
			
			dcassert(i_cur_pfm >= 0 && i_cur_pfm < (int)platforms.size());
			return platforms[i_cur_pfm];
		}
		
	private:
		vector<GPGPUPlatform *> platforms;
		int i_cur_pfm;
		
		FastCriticalSection cs;
};

class GPGPUTTHManager : public GPGPUManager, public Singleton<GPGPUTTHManager>
{
	public:
		GPGPUTTHManager();
};

class GPGPUOpenCL : public GPGPUPlatform
{
	public:
		class OCLGPGPUDevice
		{
			public:
				OCLGPGPUDevice(cl_platform_id pfm_id, cl_device_id dev_id, const string &dev_name)
					: m_pfm_id(pfm_id), m_dev_id(dev_id), m_dev_name(dev_name) {}
					
				cl_platform_id m_pfm_id;
				cl_device_id m_dev_id;
				string m_dev_name;
		};
		
		GPGPUOpenCL();
		~GPGPUOpenCL();
		
		void select_device(int num);
		
		int get_cur_dev()
		{
			CFlyLock(cs);
			return cur_dev_num;
		}
		
		const string& get_dev_name(int num)
		{
			dcassert(num >= 0 && num < (int)devs.size());
			return devs[num].m_dev_name;
		}
		
		int get_dev_cnt()
		{
			return (int)devs.size();
		}
		
		const string& get_platform_name()
		{
			return pfm_name;
		}
		
		Status get_status()
		{
			return sts;
		}
		
		bool krn_ttr(uint8_t *data, uint64_t bc, uint64_t last_bs, uint64_t res[3]);
		
	private:
		const string pfm_name;  // Name of GPGPU platform (OpenCL, CUDA, DirectCompute etc.)
		// Don't confuse with OpenCL platforms which get by
		// calling clGetPlatformIDs
		vector<OCLGPGPUDevice> devs;
		int cur_dev_num;
		
		cl_context cur_cntxt;
		cl_command_queue cur_cmd_q;
		
		cl_program cur_prog;
		cl_kernel cur_ttr_krn;
		
		Status sts;
		
		// Number of work items with witch excecute kernel (1 dim and 1 workgroup)
		size_t wis1d1wg;
		
		// Gets max work items for one workgroup one dimentional kernel excecuting
		size_t get_1d_max_wg_wi(int dev_num);
		
		CriticalSection cs;
		
		void release_cl();
};

#endif // FLYLINKDC_USE_GPU_TTH
#endif // !defined(GPGPUMANAGER_H)