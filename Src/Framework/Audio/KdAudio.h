#pragma once

#include <chrono>

#include "Application/Core/EventBus/EventBus.h"
#include "Application/Core/EventBus/Events/AudioEvents.h"
#include "KdSoundDef.h"

class KdSoundEffect;
class KdSoundInstance;
class KdSoundInstance3D;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ゲーム内の音を管理するクラス
// ・Events::Sound*Requested を購読し、Update()でまとめて処理する
// ・再生中の音は SoundHandle で管理する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdAudioManager
{
public:
	KdAudioManager() { m_categoryVolume.fill(1.0f); }
	~KdAudioManager() { Release(); }

	// 初期化(イベントの購読開始)
	void Init(const std::shared_ptr<EventBus>& bus = GlobalEventBus::Instance());

	// イベント処理・フェード・3D音の更新・終了した音の回収
	void Update();

	// 毎フレーム呼ぶため、イベントにせず直接呼ぶ
	void SetListenerMatrix(const Math::Matrix& mWorld);

	// 旧API(移行用)。新規コードは Events::PublishSoundPlayEvent / PublishSoundPlay3DEvent を使う
	std::shared_ptr<KdSoundInstance>   Play(std::string_view rName, bool loop = false);
	std::shared_ptr<KdSoundInstance3D> Play3D(std::string_view rName, const Math::Vector3& rPos, bool loop = false);

	void StopAllSound();
	void PauseAllSound();
	void ResumeAllSound();

	// 再生中の音をすべて停止し、サウンドアセットも解放する
	void SoundReset();

	const std::unique_ptr<DirectX::AudioEngine>& GetAudioEngine() { return m_audioEng; }

	const DirectX::AudioListener& GetListener() { return m_listener; }

	// 論理ID → 音の定義(JSON)を読み込む
	bool LoadSoundDefs(std::string_view path) { return m_defs.Load(path); }

	// サウンドアセットの一括読込
	void LoadSoundAssets(std::initializer_list<std::string_view> fileNames);

	void Release();

private:

	// 再生中の音1つ分の管理情報
	struct ActiveSound
	{
		std::shared_ptr<KdSoundInstance> instance;
		std::string		soundId;
		SoundCategory	category = SoundCategory::SE;
		float			defVolume = 1.0f;	// SoundDef側の音量
		float			userVolume = 1.0f;	// 要求側が指定した音量

		// フェード(fadeVolumeを最終音量に掛ける)
		float			fadeVolume = 1.0f;
		float			fadeFrom = 1.0f;
		float			fadeTo = 1.0f;
		float			fadeDuration = 0.0f;
		float			fadeElapsed = 0.0f;
		bool			fading = false;
		bool			stopOnFadeEnd = false;
	};

	// イベントハンドラ(キュー経由で Update から呼ばれる)
	void OnPlay(const Events::SoundPlayRequested& e);
	void OnStop(const Events::SoundStopRequested& e);
	void OnVolume(const Events::SoundVolumeChangeRequested& e);
	void OnFade(const Events::SoundFadeRequested& e);
	void OnCategoryVolume(const Events::SoundCategoryVolumeChangeRequested& e);

	// 受信したイベントは即処理せず、キューに積んで Update で処理する
	template<class T>
	void SubscribeQueued(void (KdAudioManager::* handler)(const T&))
	{
		m_subs.push_back(m_bus->SubscribeScoped<T>([this, handler](const T& e) {
			m_pending.push_back([this, handler, e] { (this->*handler)(e); });
			}));
	}

	// 引数なしの関数を直接呼ぶ版(StopAllSoundなど)
	template<class T>
	void SubscribeQueued(void (KdAudioManager::* handler)())
	{
		m_subs.push_back(m_bus->SubscribeScoped<T>([this, handler](const T&) {
			m_pending.push_back([this, handler] { (this->*handler)(); });
			}));
	}

	// 生成・再生・登録(イベントと旧APIの共通処理)
	std::shared_ptr<KdSoundInstance> StartSound(const Events::SoundPlayRequested& e);

	// 最終音量 = 要求音量 × 定義音量 × 種別音量 × フェード
	void ApplyVolume(ActiveSound& s);

	// 現在の音量から目標へのフェードを開始する(duration<=0なら即時)
	void StartFade(ActiveSound& s, float target, float duration, bool stopOnEnd);

	// フェードを進める
	void UpdateFade(ActiveSound& s, float deltaTime);

	// 再生中の同一IDの数(同時発音数の判定用)
	int CountActive(const std::string& soundId);

	// サウンドデータの取得orロード
	std::shared_ptr<KdSoundEffect> GetSound(std::string_view fileName);

	std::unique_ptr<DirectX::AudioEngine>	m_audioEng;
	DirectX::AudioListener					m_listener;

	std::shared_ptr<EventBus>				m_bus;
	std::vector<ScopedSubscriber>			m_subs;
	std::vector<std::function<void()>>		m_pending;

	// 再生中のサウンド(キーは SoundHandle::id)
	std::unordered_map<uint32_t, ActiveSound> m_active;

	KdSoundDefTable								m_defs;
	std::array<float, (size_t)SoundCategory::Max>	m_categoryVolume;
	std::mt19937								m_random{ std::random_device{}() };

	// フェード用の経過時間計測(ゲームのタイムスケールの影響を受けない)
	std::chrono::steady_clock::time_point	m_lastUpdate;

	std::unordered_map<std::string, std::shared_ptr<KdSoundEffect>>	m_soundMap;
};


// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 2Dサウンド再生用のインスタンス
// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdSoundInstance
{
public:
	KdSoundInstance(const std::shared_ptr<KdSoundEffect>& soundEffect);
	virtual ~KdSoundInstance() = default;

	virtual bool CreateInstance();

	virtual void Play(bool loop = false);

	// 毎フレーム呼ばれる(3Dの音像更新用)
	virtual void Update() {}

	void Stop() { if (m_instance) { m_instance->Stop(); } }
	void Pause() { if (m_instance) { m_instance->Pause(); } }
	void Resume() { if (m_instance) { m_instance->Resume(); } }

	// vol … 1.0が100%
	void SetVolume(float vol);

	// pitch … 低音-1.0～1.0高音
	void SetPitch(float pitch);

	bool IsPlaying();
	bool IsPause();
	bool IsStopped();

	KdSoundInstance(const KdSoundInstance& src) = delete;
	void operator=(const KdSoundInstance& src) = delete;

protected:

	std::unique_ptr<DirectX::SoundEffectInstance>	m_instance;

	// 元データを保持して、インスタンスより先に解放されないようにする
	std::shared_ptr<KdSoundEffect>					m_soundData;
};


// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 3Dサウンド再生用のインスタンス
// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdSoundInstance3D : public KdSoundInstance
{
public:
	KdSoundInstance3D(const std::shared_ptr<KdSoundEffect>& soundEffect, const DirectX::AudioListener& ownerListener);

	bool CreateInstance() override;

	void Play(bool loop = false) override;

	// リスナーの移動に追従して音像を更新する
	void Update() override;

	void SetPos(const Math::Vector3& rPos);

	// 減衰倍率 1:通常 FLT_MIN～FLT_MAX
	void SetCurveDistanceScaler(float val);

protected:

	DirectX::AudioEmitter			m_emitter;

	// Manager側のリスナーを参照する(Managerより長生きさせない)
	const DirectX::AudioListener& m_ownerListener;
};


// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// サウンドデータ(Wave形式)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdSoundEffect
{
public:
	KdSoundEffect() {}
	~KdSoundEffect() = default;

	std::unique_ptr<DirectX::SoundEffectInstance> CreateInstance(DirectX::SOUND_EFFECT_INSTANCE_FLAGS flag)
	{
		if (!m_soundEffect) { return nullptr; }

		return m_soundEffect->CreateInstance(flag);
	}

	bool Load(std::string_view fileName, const std::unique_ptr<DirectX::AudioEngine>& engine);

	KdSoundEffect(const KdSoundEffect& src) = delete;
	void operator=(const KdSoundEffect& src) = delete;

private:

	std::unique_ptr<DirectX::SoundEffect>	m_soundEffect;
};