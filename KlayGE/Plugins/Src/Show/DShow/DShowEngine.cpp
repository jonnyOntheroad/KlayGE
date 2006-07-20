// DShowEngine.cpp
// KlayGE DirectShow 播放引擎 实现文件
// Ver 2.0.0
// 版权所有(C) 龚敏敏, 2003
// Homepage: http://klayge.sourceforge.net
//
// 2.0.0
// 初次建立 (2003.9.3)
//
// 修改记录
/////////////////////////////////////////////////////////////////////////////////

#define _WIN32_DCOM
#include <KlayGE/KlayGE.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/Util.hpp>
#include <KlayGE/COMPtr.hpp>
#include <KlayGE/Context.hpp>

#include <boost/assert.hpp>
#include <uuids.h>

#include <KlayGE/DShow/DShowVMR9Allocator.hpp>
#include <KlayGE/DShow/DShow.hpp>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "strmiids.lib")

namespace KlayGE
{
	// 构造函数
	/////////////////////////////////////////////////////////////////////////////////
	DShowEngine::DShowEngine()
	{
		::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

		this->Init();
	}

	// 析构函数
	/////////////////////////////////////////////////////////////////////////////////
	DShowEngine::~DShowEngine()
	{
		this->Free();

		::CoUninitialize();
	}

	// 初始化
	/////////////////////////////////////////////////////////////////////////////////
	void DShowEngine::Init()
	{
		state_ = SS_Uninit;
	}

	// 释放
	/////////////////////////////////////////////////////////////////////////////////
	void DShowEngine::Free()
	{
		this->Stop();

		vmr_allocator_.reset();
		media_event_.reset();
		media_control_.reset();
		filter_.reset();
		graph_.reset();
	}

	// 播放
	/////////////////////////////////////////////////////////////////////////////////
	void DShowEngine::DoPlay()
	{
		TIF(media_control_->Run());
	}

	// 暂停播放
	/////////////////////////////////////////////////////////////////////////////////
	void DShowEngine::DoPause()
	{
		TIF(media_control_->Pause());
	}

	// 停止播放
	/////////////////////////////////////////////////////////////////////////////////
	void DShowEngine::DoStop()
	{
		TIF(media_control_->Stop());
	}

	// 载入文件
	/////////////////////////////////////////////////////////////////////////////////
	void DShowEngine::Load(std::string const & fileName)
	{
		this->Free();
		this->Init();

		IGraphBuilder* graph;
		::CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_ALL,
			IID_IGraphBuilder, reinterpret_cast<void**>(&graph));
		graph_ = MakeCOMPtr(graph);

		IBaseFilter* filter;
		::CoCreateInstance(CLSID_VideoMixingRenderer9, NULL, CLSCTX_INPROC_SERVER,
			IID_IBaseFilter, reinterpret_cast<void**>(&filter));
		filter_ = MakeCOMPtr(filter);

		boost::shared_ptr<IVMRFilterConfig9> filter_config;
		{
			IVMRFilterConfig9* tmp;
			TIF(filter_->QueryInterface(IID_IVMRFilterConfig9, reinterpret_cast<void**>(&tmp)));
			filter_config = MakeCOMPtr(tmp);
		}

		TIF(filter_config->SetRenderingMode(VMR9Mode_Renderless));
		TIF(filter_config->SetNumberOfStreams(1));

		boost::shared_ptr<IVMRSurfaceAllocatorNotify9> vmr_surf_alloc_notify;
		{
			IVMRSurfaceAllocatorNotify9* tmp;
			TIF(filter_->QueryInterface(IID_IVMRSurfaceAllocatorNotify9, reinterpret_cast<void**>(&tmp)));
			vmr_surf_alloc_notify = MakeCOMPtr(tmp);
		}

		// create our surface allocator
		vmr_allocator_ = MakeCOMPtr(new DShowVMR9Allocator(::GetActiveWindow()));

		// let the allocator and the notify know about each other
		TIF(vmr_surf_alloc_notify->AdviseSurfaceAllocator(static_cast<DWORD_PTR>(DShowVMR9Allocator::USER_ID),
			vmr_allocator_.get()));
		TIF(vmr_allocator_->AdviseNotify(vmr_surf_alloc_notify.get()));

		TIF(graph_->AddFilter(filter_.get(), L"Video Mixing Renderer 9"));

		IMediaControl* media_control;
		TIF(graph_->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&media_control)));
		media_control_ = MakeCOMPtr(media_control);

		IMediaEvent* media_event;
		TIF(graph_->QueryInterface(IID_IMediaEvent, reinterpret_cast<void**>(&media_event)));
		media_event_ = MakeCOMPtr(media_event);

		std::wstring fn;
		Convert(fn, fileName);
		TIF(graph_->RenderFile(fn.c_str(), NULL));

		state_ = SS_Stopped;
	}

	// 检查播放是否完成
	/////////////////////////////////////////////////////////////////////////////////
	bool DShowEngine::IsComplete()
	{
		long lEventCode, lParam1, lParam2;
		bool ret(false);

		HRESULT hr(media_event_->GetEvent(&lEventCode, reinterpret_cast<LONG_PTR*>(&lParam1),
							reinterpret_cast<LONG_PTR*>(&lParam2), 0));
		if (SUCCEEDED(hr))
		{
			if (1 == lEventCode)	// EC_COMPLETE
			{
				ret = true;
			}

			// 释放和这个事件相关的内存
			TIF(media_event_->FreeEventParams(lEventCode, lParam1, lParam2));
		}

		return ret;
	}

	// 获取当前状态
	/////////////////////////////////////////////////////////////////////////////////
	ShowState DShowEngine::State(long msTimeout)
	{
		OAFilterState fs;
		HRESULT hr(media_control_->GetState(msTimeout, &fs));
		if (FAILED(hr))
		{
			return SS_Unkown;
		}

		state_ = SS_Unkown;
		switch (fs)
		{
		case State_Stopped:
			state_ = SS_Stopped;
			break;

		case State_Paused:
			state_ = SS_Paused;
			break;

		case State_Running:
			state_ = SS_Playing;
			break;
		}

		return state_;
	}

	// 获取显示的纹理
	/////////////////////////////////////////////////////////////////////////////////
	TexturePtr DShowEngine::PresentTexture()
	{
		return checked_cast<DShowVMR9Allocator*>(vmr_allocator_.get())->PresentTexture();
	}
}
