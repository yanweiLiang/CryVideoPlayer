// Copyright (C) 2017-2018 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/CryVideoPlayer/blob/master/LICENSE

#include "StdAfx.h"
#include "PluginEnv.h"
#include "Plugin.h"

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/EnvPackage.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CrySchematyc/CoreAPI.h>

#include <CryCore/Platform/platform_impl.inl>

#include "Schematyc/TextureVideoPlayerComponent.h"

#include "Video/2DVideoQueue.h"
#include "Video/TextureVideoQueue.h"

#include "Input/InputDispatcher.h"

void CmdPlay2DVideo(IConsoleCmdArgs* args)
{
	if (args == nullptr)
		return;

	if (args->GetArgCount() > 1)
	{
		const char* video = args->GetArg(1);

		if (mEnv->pVideoQueue != nullptr)
		{
			mEnv->pVideoQueue->Play2DVideo(video);
		}
	}
}

void CmdPause2DVideo(IConsoleCmdArgs* args)
{
	if (mEnv->pVideoQueue != nullptr)
	{
		mEnv->pVideoQueue->Pause2DVideo();
	}
}

void CmdResume2DVideo(IConsoleCmdArgs* args)
{
	if (mEnv->pVideoQueue != nullptr)
	{
		mEnv->pVideoQueue->Resume2DVideo();
	}
}

void CmdStop2DVideo(IConsoleCmdArgs* args)
{
	if (mEnv->pVideoQueue != nullptr)
	{
		mEnv->pVideoQueue->Stop2DVideo();
	}
}

void CVideoPlugin::RegisterCVars()
{
	IConsole* pConsole = gEnv->pConsole;
	if (pConsole != nullptr && !gEnv->IsDedicated())
	{
		// Commands
		REGISTER_COMMAND("vp_play", CmdPlay2DVideo, VF_NULL, "Play video file. Ex. vp_play logo (without extension name)");
		REGISTER_COMMAND("vp_pause", CmdPause2DVideo, VF_NULL, "Pause current playing video");
		REGISTER_COMMAND("vp_resume", CmdResume2DVideo, VF_NULL, "Resume current paused video");
		REGISTER_COMMAND("vp_stop", CmdStop2DVideo, VF_NULL, "Stop current playing/paused video");

		// CVars
		REGISTER_CVAR2("vp_use_CrySDL", &mEnv->m_bUseCrySDL, 1, VF_NULL, "If you use CryAudioImplSDLMixer - Set it to 1, if not - 0");
		REGISTER_CVAR2("vp_debug_log", &mEnv->m_bDebugLog, 0, VF_NULL, "Video plugin debug log");
		REGISTER_CVAR2("vp_debug_draw", &mEnv->m_bDebugDraw, 0, VF_NULL, "Video plugin debug draw");
	}
}

void CVideoPlugin::UnRegisterCVars()
{
	IConsole* pConsole = gEnv->pConsole;
	if (pConsole != nullptr && !gEnv->IsDedicated())
	{
		pConsole->RemoveCommand("vp_play");
		pConsole->RemoveCommand("vp_pause");
		pConsole->RemoveCommand("vp_resume");
		pConsole->RemoveCommand("vp_stop");

		pConsole->UnregisterVariable("vp_use_CrySDL", true);
		pConsole->UnregisterVariable("vp_debug_log", true);
		pConsole->UnregisterVariable("vp_debug_draw", true);
	}
}

CVideoPlugin::~CVideoPlugin()
{
	if (gEnv->pSystem != nullptr && !gEnv->IsDedicated())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

#ifndef CRY_IS_MONOLITHIC_BUILD
	if (gEnv->pSchematyc != nullptr && !gEnv->IsDedicated())
	{
		gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(CVideoPlugin::GetCID());
	}
#endif

	UnRegisterCVars();

	mEnv->pThreadManager->Release();
	mEnv->pVideoQueue->Release();
	mEnv->pTextureVideoQueue->Release();

	SAFE_DELETE_11(mEnv->pThreadManager);
	SAFE_DELETE_11(mEnv->pVideoQueue);
	SAFE_DELETE_11(mEnv->pTextureVideoQueue);
	SAFE_DELETE_11(mEnv->pInputDispatcher);
	SAFE_DELETE_11(mEnv);
}

bool CVideoPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	if (gEnv->pSystem != nullptr && !gEnv->IsDedicated())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CVideoPlugin_Listener");
	}

	if (!gEnv->IsDedicated())
	{
		mEnv->pThreadManager = new CVideoPluginThreadManager();
		mEnv->pThreadManager->SpawnDecoderThread();
	}

	if (gEnv->pConsole != nullptr)
	{
		ICVar* pStartScreen = gEnv->pConsole->GetCVar("sys_rendersplashscreen");
		if (pStartScreen != nullptr)
		{
			pStartScreen->Set(0);
		}
	}

	return true;
}

void CVideoPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	if (gEnv->IsDedicated())
	{
		return;
	}

	switch (event)
	{
	case ESYSTEM_EVENT_GAME_POST_INIT:
	{
		RegisterCVars();

		if (mEnv->pInputDispatcher == nullptr)
		{
			mEnv->pInputDispatcher = new CInputDispatcher();
		}

		if (mEnv->pVideoQueue == nullptr)
		{
			mEnv->pVideoQueue = new C2DVideoQueue();
		}

		if (mEnv->pTextureVideoQueue == nullptr)
		{
			mEnv->pTextureVideoQueue = new CTextureVideoQueue();
		}

		break;
	}
	case ESYSTEM_EVENT_REGISTER_SCHEMATYC_ENV:
	{
#ifndef CRY_IS_MONOLITHIC_BUILD 
		auto staticAutoRegisterLambda = [](Schematyc::IEnvRegistrar& registrar)
		{
			Detail::CStaticAutoRegistrar<Schematyc::IEnvRegistrar&>::InvokeStaticCallbacks(registrar);
		};

		if (gEnv->pSchematyc)
		{
			gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(
				stl::make_unique<Schematyc::CEnvPackage>(
					CVideoPlugin::GetCID(),
					"EntityComponents",
					"Ilya Chernetsov",
					"Components",
					staticAutoRegisterLambda
					)
			);
		}
#endif
		break;
	}
	default:
		break;
	}
}

void CVideoPlugin::Play2DVideo(const char * videoName, bool preload, bool looped, int audioTrack, bool isSkippable, bool canBePaused, IVideoPlayerEventListener* pVideoPlayerListener)
{
	if (mEnv != nullptr && mEnv->pVideoQueue != nullptr)
	{
		mEnv->pVideoQueue->Play2DVideo(videoName, preload, looped, audioTrack, isSkippable, canBePaused);

		if (pVideoPlayerListener != nullptr)
		{
			mEnv->pVideoQueue->Register2DVideoPlayerListener(pVideoPlayerListener);
		}
	}
	else
	{
		LogError("<CVideoPlugin> Can't play 2D video video queue == nullptr!");
	}
}

void CVideoPlugin::Pause2DVideo()
{
	if (mEnv != nullptr && mEnv->pVideoQueue != nullptr)
	{
		mEnv->pVideoQueue->Pause2DVideo();
	}
	else
	{
		LogError("<CVideoPlugin> Can't pause 2D video video queue == nullptr!");
	}
}

void CVideoPlugin::Resume2DVideo()
{
	if (mEnv != nullptr && mEnv->pVideoQueue != nullptr)
	{
		mEnv->pVideoQueue->Resume2DVideo();
	}
	else
	{
		LogError("<CVideoPlugin> Can't resume 2D video video queue == nullptr!");
	}
}

void CVideoPlugin::Stop2DVideo()
{
	if (mEnv != nullptr && mEnv->pVideoQueue != nullptr)
	{
		mEnv->pVideoQueue->Stop2DVideo();
	}
	else
	{
		LogError("<CVideoPlugin> Can't stop 2D video video queue == nullptr!");
	}
}

bool CVideoPlugin::Is2DVideoCurrentlyPlaying()
{
	if (mEnv != nullptr && mEnv->pVideoQueue != nullptr)
	{
		return mEnv->pVideoQueue->Is2DVideoPlayingNow();
	}
	else
	{
		LogError("<CVideoPlugin> Can't get 2D video info because video queue == nullptr!");
	}

	return false;
}

CRYREGISTER_SINGLETON_CLASS(CVideoPlugin)