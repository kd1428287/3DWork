#include "KdAudio.h"

// ##### KdAudioManager #####

void KdAudioManager::Init(const std::shared_ptr<EventBus>& bus)
{
	m_subs.clear();
	m_bus = bus;

	DirectX::AUDIO_ENGINE_FLAGS eflags = DirectX::AudioEngine_ReverbUseFilters;

	m_audioEng = std::make_unique<DirectX::AudioEngine>(eflags);
	m_audioEng->SetReverb(DirectX::Reverb_Default);

	m_listener.OrientFront = { 0, 0, 1 };

	SubscribeQueued<Events::SoundPlayRequested>(&KdAudioManager::OnPlay);
	SubscribeQueued<Events::SoundStopRequested>(&KdAudioManager::OnStop);
	SubscribeQueued<Events::SoundVolumeChangeRequested>(&KdAudioManager::OnVolume);
	SubscribeQueued<Events::SoundCategoryVolumeChangeRequested>(&KdAudioManager::OnCategoryVolume);

	SubscribeQueued<Events::StopAllSoundRequested>(&KdAudioManager::StopAllSound);
	SubscribeQueued<Events::PauseAllSoundRequested>(&KdAudioManager::PauseAllSound);
	SubscribeQueued<Events::ResumeAllSoundRequested>(&KdAudioManager::ResumeAllSound);
	SubscribeQueued<Events::SoundResetRequested>(&KdAudioManager::SoundReset);
}

void KdAudioManager::Update()
{
	// 処理中に積まれた分は次フレームに回す
	std::vector<std::function<void()>> pending;
	pending.swap(m_pending);

	for (auto& command : pending) { command(); }

	if (m_audioEng)
	{
		// デバイス喪失時は再初期化を試みる
		if (!m_audioEng->Update() && m_audioEng->IsCriticalError())
		{
			m_audioEng->Reset();
		}
	}

	std::vector<SoundHandle> finished;

	for (auto it = m_active.begin(); it != m_active.end();)
	{
		it->second.instance->Update();

		if (it->second.instance->IsStopped())
		{
			finished.push_back(SoundHandle{ it->first });
			it = m_active.erase(it);

			continue;
		}

		++it;
	}

	// 回収後に通知する(ハンドラ内で再生要求されても安全)
	if (m_bus)
	{
		for (SoundHandle handle : finished)
		{
			Events::SoundFinished e;
			e.handle = handle;

			m_bus->Publish(e);
		}
	}
}

void KdAudioManager::SetListenerMatrix(const Math::Matrix& mWorld)
{
	m_listener.SetPosition(mWorld.Translation());

	m_listener.OrientFront = mWorld.Backward();
}

// ///// イベントハンドラ /////

void KdAudioManager::OnPlay(const Events::SoundPlayRequested& e)
{
	StartSound(e);
}

void KdAudioManager::OnStop(const Events::SoundStopRequested& e)
{
	auto it = m_active.find(e.handle.id);

	if (it != m_active.end()) { it->second.instance->Stop(); }
}

void KdAudioManager::OnVolume(const Events::SoundVolumeChangeRequested& e)
{
	auto it = m_active.find(e.handle.id);

	if (it == m_active.end()) { return; }

	it->second.userVolume = e.volume;

	ApplyVolume(it->second);
}

void KdAudioManager::OnCategoryVolume(const Events::SoundCategoryVolumeChangeRequested& e)
{
	m_categoryVolume[(size_t)e.category] = e.volume;

	// 再生中の音にも反映する
	for (auto& kv : m_active)
	{
		if (kv.second.category == e.category) { ApplyVolume(kv.second); }
	}
}

// ///// 再生 /////

std::shared_ptr<KdSoundInstance> KdAudioManager::StartSound(const Events::SoundPlayRequested& e)
{
	if (!m_audioEng) { return nullptr; }

	// 未登録のIDはファイル名として扱う(移行用)
	static const SoundDef s_defaultDef;
	const SoundDef* pDef = m_defs.Find(e.soundId);
	const SoundDef& def = pDef ? *pDef : s_defaultDef;

	// 同時発音数の上限を超える要求は捨てる
	if (def.maxInstances > 0 && CountActive(e.soundId) >= def.maxInstances) { return nullptr; }

	std::string file = e.soundId;

	if (pDef)
	{
		// 複数ファイルがあればランダムに選ぶ
		std::uniform_int_distribution<size_t> dist(0, def.files.size() - 1);
		file = def.files[dist(m_random)];
	}

	std::shared_ptr<KdSoundEffect> soundData = GetSound(file);

	if (!soundData) { return nullptr; }

	std::shared_ptr<KdSoundInstance> instance;
	std::shared_ptr<KdSoundInstance3D> instance3D;

	if (e.is3D)
	{
		instance3D = std::make_shared<KdSoundInstance3D>(soundData, m_listener);
		instance = instance3D;
	}
	else
	{
		instance = std::make_shared<KdSoundInstance>(soundData);
	}

	if (!instance->CreateInstance()) { return nullptr; }

	float pitch = e.pitch;

	if (def.pitchRandom > 0.0f)
	{
		std::uniform_real_distribution<float> dist(-def.pitchRandom, def.pitchRandom);
		pitch += dist(m_random);
	}

	instance->SetPitch(std::clamp(pitch, -1.0f, 1.0f));

	ActiveSound active;
	active.instance = instance;
	active.soundId = e.soundId;
	active.category = def.category;
	active.defVolume = def.volume;
	active.userVolume = e.volume;

	ApplyVolume(active);

	instance->Play(e.loop || def.loop);

	if (instance3D) { instance3D->SetPos(e.pos); }

	// ハンドル未指定(旧API・発行忘れ)ならここで発行する
	SoundHandle handle = e.handle.IsValid() ? e.handle : SoundHandle::Issue();

	m_active[handle.id] = std::move(active);

	return instance;
}

void KdAudioManager::ApplyVolume(ActiveSound& s)
{
	s.instance->SetVolume(s.userVolume * s.defVolume * m_categoryVolume[(size_t)s.category]);
}

int KdAudioManager::CountActive(const std::string& soundId)
{
	int count = 0;

	for (auto& kv : m_active)
	{
		if (kv.second.soundId == soundId && !kv.second.instance->IsStopped()) { ++count; }
	}

	return count;
}

std::shared_ptr<KdSoundInstance> KdAudioManager::Play(std::string_view rName, bool loop)
{
	Events::SoundPlayRequested e;
	e.soundId = rName;
	e.loop = loop;

	return StartSound(e);
}

std::shared_ptr<KdSoundInstance3D> KdAudioManager::Play3D(std::string_view rName, const Math::Vector3& rPos, bool loop)
{
	Events::SoundPlayRequested e;
	e.soundId = rName;
	e.pos = rPos;
	e.is3D = true;
	e.loop = loop;

	return std::static_pointer_cast<KdSoundInstance3D>(StartSound(e));
}

// ///// 一括操作 /////

void KdAudioManager::StopAllSound()
{
	for (auto& kv : m_active) { kv.second.instance->Stop(); }
}

void KdAudioManager::PauseAllSound()
{
	for (auto& kv : m_active) { kv.second.instance->Pause(); }
}

void KdAudioManager::ResumeAllSound()
{
	for (auto& kv : m_active) { kv.second.instance->Resume(); }
}

void KdAudioManager::SoundReset()
{
	StopAllSound();

	// インスタンスを先に解放してからアセットを解放する
	m_active.clear();
	m_soundMap.clear();
}

// ///// アセット /////

void KdAudioManager::LoadSoundAssets(std::initializer_list<std::string_view> fileNames)
{
	for (std::string_view fileName : fileNames)
	{
		if (!GetSound(fileName))
		{
			assert(0 && "LoadSoundAssets:ファイルを読み込めません。名前を確認してください。");
		}
	}
}

std::shared_ptr<KdSoundEffect> KdAudioManager::GetSound(std::string_view fileName)
{
	// string_viewは終端が保証されないので文字列にしてから検索する
	const std::string key(fileName);

	auto itFound = m_soundMap.find(key);

	if (itFound != m_soundMap.end()) { return itFound->second; }

	auto newSound = std::make_shared<KdSoundEffect>();

	if (!newSound->Load(fileName, m_audioEng)) { return nullptr; }

	m_soundMap.emplace(key, newSound);

	return newSound;
}

void KdAudioManager::Release()
{
	// 先に購読を解除して、破棄中にイベントを受けないようにする
	m_subs.clear();
	m_pending.clear();

	StopAllSound();

	m_active.clear();
	m_soundMap.clear();

	m_audioEng = nullptr;
	m_bus = nullptr;
}


// ##### KdSoundEffect #####

bool KdSoundEffect::Load(std::string_view fileName, const std::unique_ptr<DirectX::AudioEngine>& engine)
{
	if (!engine) { return false; }

	try
	{
		std::wstring wFilename = sjis_to_wide(std::string(fileName).c_str());

		m_soundEffect = std::make_unique<DirectX::SoundEffect>(engine.get(), wFilename.c_str());
	}
	catch (...)
	{
		assert(0 && "Sound File Load Error");

		return false;
	}

	return true;
}


// ##### KdSoundInstance #####

KdSoundInstance::KdSoundInstance(const std::shared_ptr<KdSoundEffect>& soundEffect)
	:m_soundData(soundEffect) {}

bool KdSoundInstance::CreateInstance()
{
	if (!m_soundData) { return false; }

	DirectX::SOUND_EFFECT_INSTANCE_FLAGS flags = DirectX::SoundEffectInstance_Default;

	m_instance = m_soundData->CreateInstance(flags);

	return m_instance != nullptr;
}

void KdSoundInstance::Play(bool loop)
{
	if (!m_instance) { return; }

	// 再生状態が不明なので一度停止してから再生する
	Stop();

	m_instance->Play(loop);
}

void KdSoundInstance::SetVolume(float vol)
{
	if (!m_instance) { return; }

	m_instance->SetVolume(vol);
}

void KdSoundInstance::SetPitch(float pitch)
{
	if (!m_instance) { return; }

	m_instance->SetPitch(pitch);
}

bool KdSoundInstance::IsPlaying()
{
	if (!m_instance) { return false; }

	return (m_instance->GetState() == DirectX::SoundState::PLAYING);
}

bool KdSoundInstance::IsPause()
{
	if (!m_instance) { return false; }

	return (m_instance->GetState() == DirectX::SoundState::PAUSED);
}

bool KdSoundInstance::IsStopped()
{
	// インスタンスが無い場合は終了扱いにして、リストから回収されるようにする
	if (!m_instance) { return true; }

	return (m_instance->GetState() == DirectX::SoundState::STOPPED);
}


// ##### KdSoundInstance3D #####

KdSoundInstance3D::KdSoundInstance3D(const std::shared_ptr<KdSoundEffect>& soundEffect, const DirectX::AudioListener& ownerListener)
	:KdSoundInstance(soundEffect), m_ownerListener(ownerListener) {}

bool KdSoundInstance3D::CreateInstance()
{
	if (!m_soundData) { return false; }

	DirectX::SOUND_EFFECT_INSTANCE_FLAGS flags = DirectX::SoundEffectInstance_Default |
		DirectX::SoundEffectInstance_Use3D | DirectX::SoundEffectInstance_ReverbUseFilters;

	m_instance = m_soundData->CreateInstance(flags);

	return m_instance != nullptr;
}

void KdSoundInstance3D::Play(bool loop)
{
	KdSoundInstance::Play(loop);
}

void KdSoundInstance3D::Update()
{
	if (!m_instance) { return; }

	m_instance->Apply3D(m_ownerListener, m_emitter, false);
}

void KdSoundInstance3D::SetPos(const Math::Vector3& rPos)
{
	if (!m_instance) { return; }

	m_emitter.SetPosition(rPos);

	m_instance->Apply3D(m_ownerListener, m_emitter, false);
}

void KdSoundInstance3D::SetCurveDistanceScaler(float val)
{
	if (!m_instance) { return; }

	m_emitter.CurveDistanceScaler = val;

	m_instance->Apply3D(m_ownerListener, m_emitter, false);
}
